import json
from flask import Flask, jsonify, request
import subprocess
import os
from flask_cors import CORS

# Initialize Flask app
app = Flask(__name__)
CORS(app)  # enable frontend communication

# --- CONFIGURATION ---
# Path to your compiled C++ executable
CPP_EXECUTABLE_PATH = './main.exe'   # 👈 matches your file name

# --- Helper Function to run the C++ backend ---
def run_cpp_command(args):
    """
    Runs the compiled C++ program with given arguments and expects clean JSON output.
    """
    command = [CPP_EXECUTABLE_PATH] + args

    if not os.path.exists(CPP_EXECUTABLE_PATH):
        return {"error": "main.exe not found. Please compile main.cpp first."}, 500

    try:
        result = subprocess.run(
            command,
            capture_output=True,
            text=True,
            timeout=10
        )
        output = result.stdout.strip()

        if not output:
            return {"error": "C++ returned no output."}, 500

        # Try to parse JSON output
        try:
            data = json.loads(output)
            return data, 200
        except json.JSONDecodeError:
            return {"error": "Invalid JSON from C++", "raw_output": output}, 500

    except subprocess.TimeoutExpired:
        return {"error": "C++ execution timed out."}, 504
    except Exception as e:
        return {"error": f"Unexpected error: {str(e)}"}, 500


# --- ROUTES ---

@app.route('/')
def index():
    return jsonify({
        "status": "API running",
        "message": "E-Commerce Recommendation System API connected with C++ backend."
    })
@app.route("/get/products", methods=["GET"]) # <-- Your current route
@app.route("/getProducts", methods=["GET"])  # <-- ADD THIS ROUTE
def get_products():
    output = run_cpp_command(["--get", "products"])
    print(f"DEBUG: Raw C++ Output for /get/products: {output}") 
    try:
        return jsonify(json.loads(output))
    except Exception as e: 
        print(f"DEBUG: JSON Parsing Error: {e}")
        return jsonify({"error": "Failed to parse C++ output", "raw": output}), 500



@app.route('/getUsers', methods=['GET'])
def get_users():
    """
    Gets user list from C++ (expects JSON output).
    Example: ./main.exe getUsers
    """
    data, status = run_cpp_command(['getUsers'])
    return jsonify(data), status

@app.route('/purchase', methods=['POST'])
def purchase():
    """
    Handles purchase: { "userId": 1, "productId": 101 }
    Passes it to C++ as: ./main.exe purchase 1 101
    """
    body = request.get_json()
    if not body or 'userId' not in body or 'productId' not in body:
        return jsonify({"error": "Missing userId or productId"}), 400
    
    userId = str(body['userId'])
    productId = str(body['productId'])

    data, status = run_cpp_command(['purchase', userId, productId])
    return jsonify(data), status

@app.route('/rate', methods=['POST'])
def rate():
    """
    Handles rating: { "userId": 1, "productId": 101, "rating": 4 }
    Passes it to C++ as: ./main.exe rate 1 101 4
    """
    body = request.get_json()
    if not body or 'userId' not in body or 'productId' not in body or 'rating' not in body:
        return jsonify({"error": "Missing userId, productId, or rating"}), 400

    userId = str(body['userId'])
    productId = str(body['productId'])
    rating = str(body['rating'])

    data, status = run_cpp_command(['rate', userId, productId, rating])
    return jsonify(data), status

@app.route('/recommend', methods=['GET'])
def recommend():
    """
    Generates recommendations for a user.
    Calls: ./main.exe recommend <userId>
    """
    userId = request.args.get('userId')
    if userId is None:
        return jsonify({"error": "Missing userId"}), 400

    data, status = run_cpp_command(['recommend', str(userId)])
    return jsonify(data), status


# --- Run Flask App ---
if __name__ == '__main__':
    app.run(debug=True, port=5000)
