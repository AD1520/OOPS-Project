import json
from flask import Flask, jsonify, request
import subprocess
import os
from flask_cors import CORS

# Initialize Flask app
app = Flask(__name__)
CORS(app)

# --- Helper Function to run the C++ backend ---
def run_cpp_command(args):
    """
    Runs the compiled C++ program with given arguments and expects clean JSON output.
    Returns (data, status_code).
    """
    # Determine the directory of the currently running server.py script
    base_dir = os.path.dirname(os.path.abspath(__file__))

    # 1. Check for common executable names/paths using the absolute base directory
    executable_path = ''
    possible_names = [
        'main.exe',
        'recommendation_system.exe',
        'main', # For Linux/macOS compile output
        'recommendation_system' # For Linux/macOS compile output
    ]
    
    for name in possible_names:
        full_path = os.path.join(base_dir, name)
        if os.path.exists(full_path):
            # FOUND IT! Use the absolute path for robustness.
            executable_path = full_path
            break

    if not executable_path:
        print(f"ERROR: C++ executable not found.")
        print(f"Checked directory: {base_dir}")
        print("Please ensure your compiled file (main.exe, main, recommendation_system.exe, or recommendation_system) is in the same directory as server.py.")
        return {"error": "C++ executable not found. Please compile main.cpp first."}, 500

    command = [executable_path] + args

    try:
        # NOTE: Using text=True and encoding='utf-8' for clean text capture
        result = subprocess.run(
            command,
            capture_output=True,
            text=True,
            encoding='utf-8', 
            timeout=10
        )
        output = result.stdout.strip()
        
        # Debugging line to see C++ output
        print(f"DEBUG: Command: {command}")
        print(f"DEBUG: Raw C++ Output: {output}")

        if not output:
            # If C++ returns nothing, check stderr for errors
            error_output = result.stderr.strip()
            if error_output:
                return {"error": "C++ execution failed (check server logs)", "stderr": error_output}, 500
            return {"error": "C++ returned no output."}, 500

        # Try to parse JSON output
        try:
            data = json.loads(output)
            return data, 200
        except json.JSONDecodeError:
            # This happens if C++ outputs non-JSON text (like an error message or status line)
            return {"error": "Invalid JSON from C++", "raw_output": output}, 500

    except subprocess.TimeoutExpired:
        return {"error": "C++ execution timed out."}, 504
    except Exception as e:
        return {"error": f"Unexpected Python error: {str(e)}"}, 500


# --- ROUTES ---

@app.route('/')
def index():
    return jsonify({
        "status": "API running",
        "message": "E-Commerce Recommendation System API connected with C++ backend."
    })
    
# --- GET ROUTES ---

@app.route("/get/products", methods=["GET"])
@app.route("/getProducts", methods=["GET"])
def get_products():
    # C++ command: ./main.exe --get products
    data, status = run_cpp_command(["--get", "products"])
    return jsonify(data), status

@app.route('/get/users', methods=['GET'])
@app.route('/getUsers', methods=['GET'])
def get_users():
    # C++ command: ./main.exe --get users
    data, status = run_cpp_command(['--get', 'users'])
    return jsonify(data), status

@app.route('/get/reviews/<int:product_id>', methods=['GET'])
@app.route('/getReviews/<int:product_id>', methods=['GET'])
def get_reviews(product_id):
    # C++ command: ./main.exe --get reviews <product_id> (NOTE: C++ code does not support 'reviews' yet)
    data, status = run_cpp_command(['--get', 'reviews', str(product_id)])
    return jsonify(data), status

# --- ADD/ACTION ROUTES ---

@app.route('/add/user', methods=['POST'])
@app.route('/addUser', methods=['POST'])
def add_user():
    try:
        data = request.get_json()
        if not data or 'name' not in data:
            return jsonify({"error": "Missing user name in request body."}), 400
            
        user_name = data['name']
        
        # C++ command: --add-user <name>
        data, status = run_cpp_command(["--add-user", user_name])
        return jsonify(data), status
    except Exception as e:
        return jsonify({"error": f"Invalid JSON input or processing error: {str(e)}"}), 400

@app.route('/purchase', methods=['POST'])
def purchase():
    try:
        body = request.get_json()
        if not body or 'userId' not in body or 'productId' not in body:
            return jsonify({"error": "Missing userId or productId"}), 400
        
        userId = str(body['userId'])
        productId = str(body['productId'])
        
        # C++ command: --purchase <userId> <productId>
        data, status = run_cpp_command(['--purchase', userId, productId])
        return jsonify(data), status
    except Exception as e:
        return jsonify({"error": f"Invalid JSON input or processing error: {str(e)}"}), 400


@app.route('/rate', methods=['POST'])
def rate():
    try:
        body = request.get_json()
        if not body or 'userId' not in body or 'productId' not in body or 'rating' not in body:
            return jsonify({"error": "Missing userId, productId, or rating"}), 400

        userId = str(body['userId'])
        productId = str(body['productId'])
        rating = str(body['rating'])

        # C++ command: --rate <userId> <productId> <rating>
        data, status = run_cpp_command(['--rate', userId, productId, rating])
        return jsonify(data), status
    except Exception as e:
        return jsonify({"error": f"Invalid JSON input or processing error: {str(e)}"}), 400


@app.route('/recommend', methods=['GET'])
def recommend():
    userId = request.args.get('userId')
    if userId is None:
        return jsonify({"error": "Missing userId"}), 400
    
    # C++ command: --recommend <userId>
    data, status = run_cpp_command(['--recommend', str(userId)])
    return jsonify(data), status

@app.route('/add/product', methods=['POST'])
@app.route('/addProduct', methods=['POST'])
def add_product():
    try:
        data = request.get_json()
        if not data or 'name' not in data or 'price' not in data:
            return jsonify({"error": "Missing product name or price in request body."}), 400
            
        product_name = data['name']
        product_price = str(data['price'])
        category = data.get('category', 'General')
        
        # C++ expects: --add-product <name> <category> <price> (note the order!)
        result, status = run_cpp_command(["--add-product", product_name, category, product_price])
        return jsonify(result), status
    except Exception as e:
        return jsonify({"error": f"Invalid JSON input or processing error: {str(e)}"}), 400
    
@app.route('/delete/user/<int:user_id>', methods=['DELETE'])
@app.route('/deleteUser/<int:user_id>', methods=['DELETE'])
def delete_user(user_id):
    try:
        # C++ command: --delete-user <userId>
        data, status = run_cpp_command(['--delete-user', str(user_id)])
        return jsonify(data), status
    except Exception as e:
        return jsonify({"error": f"Error deleting user: {str(e)}"}), 400


@app.route('/delete/product/<int:product_id>', methods=['DELETE'])
@app.route('/deleteProduct/<int:product_id>', methods=['DELETE'])
def delete_product(product_id):
    try:
        # C++ command: --delete-product <productId>
        data, status = run_cpp_command(['--delete-product', str(product_id)])
        return jsonify(data), status
    except Exception as e:
        return jsonify({"error": f"Error deleting product: {str(e)}"}), 400
# --- Run Flask App ---
if __name__ == '__main__':
    app.run(debug=True, port=5000)
