#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <sstream> 
#include <stdexcept> 
#include <limits> // Required for numeric_limits

using namespace std;

// --- Constants ---
const string PRODUCTS_FILE = "products.json";
const string USERS_FILE = "users.json";
const string REVIEWS_FILE = "reviews.json";

// --- Utility Functions for JSON and String Parsing ---

/**
 * Escapes special characters in a string for safe JSON inclusion.
 */
string escapeJsonString(const string& s) {
    string escaped;
    for (char c : s) {
        switch (c) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += c;
        }
    }
    return escaped;
}

/**
 * Simple helper to extract a value associated with a key from a raw JSON string.
 * This is a highly simplified parser, assuming the key/value pair is flat.
 */
string extractJsonValue(const string& json, const string& key) {
    string search = "\"" + key + "\":";
    size_t start = json.find(search);
    if (start == string::npos) return "";

    start += search.length();
    
    // Skip whitespace
    while (start < json.length() && isspace(json[start])) start++;

    if (start >= json.length()) return "";

    // If it's a string value
    if (json[start] == '"') {
        start++;
        size_t end = json.find('"', start);
        if (end != string::npos) return json.substr(start, end - start);
    } 
    // If it's a numeric or boolean value
    else {
        size_t end = json.find_first_of(" \t\n\r,}", start);
        if (end != string::npos) return json.substr(start, end - start);
    }
    return "";
}


// --- 1. Review Class ---
class Review {
private:
    int user_id;
    int product_id;
    int rating; // 1-5
    string comment;

public:
    Review(int uid, int pid, int r, const string& c) 
        : user_id(uid), product_id(pid), rating(r), comment(c) {}

    // Getters
    int getUserId() const { return user_id; }
    int getProductId() const { return product_id; }
    int getRating() const { return rating; }
    string getComment() const { return comment; }

    // JSON serialization
    string toJson() const {
        ostringstream oss;
        oss << "{"
            << "\"user_id\":" << user_id << ","
            << "\"product_id\":" << product_id << ","
            << "\"rating\":" << rating << ","
            << "\"comment\":\"" << escapeJsonString(comment) << "\""
            << "}";
        return oss.str();
    }
};

// --- 2. Product Class ---
class Product {
private:
    int id;
    string name;
    string category;
    double price;

public:
    Product(int id, const string& name, const string& category, double price)
        : id(id), name(name), category(category), price(price) {}

    // Getters
    int getId() const { return id; }
    string getName() const { return name; }
    string getCategory() const { return category; }
    double getPrice() const { return price; }

    // JSON serialization (without rating, as it's calculated externally)
    string toJson() const {
        ostringstream oss;
        oss << fixed << setprecision(2);
        oss << "{"
            << "\"id\":" << id << ","
            << "\"name\":\"" << escapeJsonString(name) << "\","
            << "\"category\":\"" << escapeJsonString(category) << "\","
            << "\"price\":" << price
            << "}";
        return oss.str();
    }
};

// --- 3. User Class ---
class User {
private:
    int id;
    string name;

public:
    User(int id, const string& name) : id(id), name(name) {}

    // Getters
    int getId() const { return id; }
    string getName() const { return name; }
    
    // JSON serialization
    string toJson() const {
        ostringstream oss;
        oss << "{"
            << "\"id\":" << id << ","
            << "\"name\":\"" << escapeJsonString(name) << "\""
            << "}";
        return oss.str();
    }
};

// --- 4. RecommendationSystem Class ---
class RecommendationSystem {
private:
    vector<Product> products;
    vector<User> users;
    vector<Review> reviews;
    int nextProductId = 1000;
    int nextUserId = 100;

    // --- Persistence Methods ---

    /** Reads all data from JSON files into memory. */
    void loadData() {
        products.clear();
        users.clear();
        reviews.clear();
        
        // Helper to read all lines from a file
        auto readAll = [](const string& filename) {
            ifstream ifs(filename);
            if (!ifs.is_open()) return string("[]"); // Return empty array if file missing
            return string((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
        };
        
        // Simple JSON array to vector parser
        auto parseArray = [](const string& raw_json) {
            vector<string> items;
            // Assumes input is [{}, {}, ...]
            if (raw_json.empty() || raw_json.size() < 2 || raw_json[0] != '[' || raw_json.back() != ']') {
                return items;
            }
            string content = raw_json.substr(1, raw_json.size() - 2); // Remove []
            size_t start = 0;
            int bracket_count = 0;
            for(size_t i = 0; i < content.length(); ++i) {
                if(content[i] == '{') bracket_count++;
                else if(content[i] == '}') bracket_count--;
                
                if(bracket_count == 0 && content[i] == ',') {
                    items.push_back(content.substr(start, i - start));
                    start = i + 1;
                }
            }
            if(start < content.length()) items.push_back(content.substr(start));
            
            // Clean up surrounding whitespace
            for(string& item : items) {
                size_t first = item.find_first_not_of(" \t\n\r");
                size_t last = item.find_last_not_of(" \t\n\r");
                if (string::npos != first) {
                    item = item.substr(first, (last - first + 1));
                }
            }
            return items;
        };

        // 1. Load Products
        for (const auto& raw_obj : parseArray(readAll(PRODUCTS_FILE))) {
            try {
                int id = stoi(extractJsonValue(raw_obj, "id"));
                double price = stod(extractJsonValue(raw_obj, "price"));
                products.emplace_back(id, extractJsonValue(raw_obj, "name"), 
                                      extractJsonValue(raw_obj, "category"), price);
                nextProductId = max(nextProductId, id + 1);
            } catch (...) {}
        }
        
        // 2. Load Users
        for (const auto& raw_obj : parseArray(readAll(USERS_FILE))) {
            try {
                int id = stoi(extractJsonValue(raw_obj, "id"));
                users.emplace_back(id, extractJsonValue(raw_obj, "name"));
                nextUserId = max(nextUserId, id + 1);
            } catch (...) {}
        }

        // 3. Load Reviews
        for (const auto& raw_obj : parseArray(readAll(REVIEWS_FILE))) {
            try {
                int uid = stoi(extractJsonValue(raw_obj, "user_id"));
                int pid = stoi(extractJsonValue(raw_obj, "product_id"));
                int rating = stoi(extractJsonValue(raw_obj, "rating"));
                reviews.emplace_back(uid, pid, rating, extractJsonValue(raw_obj, "comment"));
            } catch (...) {}
        }
    }

    /** Writes all in-memory data back to JSON files. */
    void saveData() const {
        // Helper to write a vector of JSON objects to a file
        auto writeVector = [](const string& filename, const auto& vec) {
            ofstream ofs(filename);
            ofs << "[" << endl;
            for (size_t i = 0; i < vec.size(); ++i) {
                ofs << vec[i].toJson();
                if (i < vec.size() - 1) ofs << ",";
                ofs << endl;
            }
            ofs << "]";
        };

        writeVector(PRODUCTS_FILE, products);
        writeVector(USERS_FILE, users);
        writeVector(REVIEWS_FILE, reviews);
    }
    
    // Helper to calculate the average rating for a product
    double calculateAverageRating(int productId) const {
        int sum = 0;
        int count = 0;
        for (const auto& review : reviews) {
            if (review.getProductId() == productId) {
                sum += review.getRating();
                count++;
            }
        }
        return count > 0 ? static_cast<double>(sum) / count : 0.0;
    }

    // Helper functions (Finders)
    const Product* findProductById(int productId) const {
        for (const auto& product : products) {
            if (product.getId() == productId) return &product;
        }
        return nullptr;
    }
    const User* findUserById(int userId) const {
        for (const auto& user : users) {
            if (user.getId() == userId) return &user;
        }
        return nullptr;
    }
    
    // Helper to check if a user has purchased a product (simplified to just check if they left a review)
    bool hasUserReviewed(int userId, int productId) const {
        for (const auto& review : reviews) {
            if (review.getUserId() == userId && review.getProductId() == productId) {
                return true;
            }
        }
        return false;
    }

public:
    // Constructor
    RecommendationSystem() {
        loadData(); // Load existing data upon initialization
    }

    // --- JSON Getters (Read Operations) ---

    string getProductsJson() const {
        ostringstream oss;
        oss << "{\"products\":[";
        for (size_t i = 0; i < products.size(); ++i) {
            const auto& p = products[i];
            oss << p.toJson();
            // Add rating information
            oss << ", \"avg_rating\":" << fixed << setprecision(2) << calculateAverageRating(p.getId());
            oss << ", \"reviews_count\":" << count_if(reviews.begin(), reviews.end(), 
                                      [&p](const Review& r){ return r.getProductId() == p.getId(); });
            oss << "}";

            if (i < products.size() - 1) oss << ",";
        }
        oss << "]}";
        return oss.str();
    }

    string getUsersJson() const {
        ostringstream oss;
        oss << "{\"users\":[";
        for (size_t i = 0; i < users.size(); ++i) {
            oss << users[i].toJson();
            if (i < users.size() - 1) oss << ",";
        }
        oss << "]}";
        return oss.str();
    }

    string getReviewsJson(int productId) const {
        ostringstream oss;
        oss << "{\"product_id\":" << productId << ", \"reviews\":[";
        bool first = true;
        for (const auto& review : reviews) {
            if (review.getProductId() == productId) {
                if (!first) oss << ",";
                oss << review.toJson();
                first = false;
            }
        }
        oss << "]}";
        return oss.str();
    }

    // --- JSON Adders (Create/Update Operations) ---

    string addUser(const string& name) {
        // Re-load data to get the latest state before modifying (essential for multi-process safety)
        loadData(); 
        
        int newId = nextUserId++;
        users.emplace_back(newId, name);
        saveData(); // Save changes
        
        return "{\"status\":\"success\", \"message\":\"User added.\", \"id\":" + to_string(newId) + "}";
    }

    string addProduct(const string& name, const string& category, double price) {
        loadData();

        int newId = nextProductId++;
        products.emplace_back(newId, name, category, price);
        saveData();

        return "{\"status\":\"success\", \"message\":\"Product added.\", \"id\":" + to_string(newId) + "}";
    }

    string addReview(int userId, int productId, int rating, const string& comment) {
        loadData();

        if (findUserById(userId) == nullptr) return "{\"status\":\"error\", \"message\":\"User not found.\"}";
        if (findProductById(productId) == nullptr) return "{\"status\":\"error\", \"message\":\"Product not found.\"}";
        if (rating < 1 || rating > 5) return "{\"status\":\"error\", \"message\":\"Invalid rating (1-5).\"}";
        
        // Check if user has already reviewed this product (simple check)
        // In a real system, we'd check purchase history here.
        if (hasUserReviewed(userId, productId)) return "{\"status\":\"error\", \"message\":\"User has already reviewed this product.\"}";


        reviews.emplace_back(userId, productId, rating, comment);
        saveData();

        return "{\"status\":\"success\", \"message\":\"Review added.\", \"product_id\":" + to_string(productId) + ", \"new_avg_rating\":" + to_string(calculateAverageRating(productId)) + "}";
    }

 
};

// --- MAIN ENTRY POINT ---
int main(int argc, char* argv[]) {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    // Initialize system. This will automatically load existing JSON data.
    RecommendationSystem system;

    if (argc < 2) {
        cout << "{\"error\": \"No command provided. Usage: ./main <command> [args...]\"}" << endl;
        return 1;
    }

    string command = argv[1];
    string output_json = "{\"error\": \"Invalid command or missing parameters.\"}";
    int exit_code = 1;

    try {
        if (command == "--get" && argc == 3) {
            // --get products | --get users
            string resource = argv[2];
            if (resource == "products") { output_json = system.getProductsJson(); exit_code = 0; } 
            else if (resource == "users") { output_json = system.getUsersJson(); exit_code = 0; }
        }
        else if (command == "--get" && argc == 4 && string(argv[2]) == "reviews") {
            // --get reviews <product_id>
            output_json = system.getReviewsJson(stoi(argv[3]));
            exit_code = 0;
        }
        else if (command == "--add" && argc == 4 && string(argv[2]) == "user") {
            // --add user '{"name": "New User"}'
            string raw_json = argv[3];
            string name = extractJsonValue(raw_json, "name");
            if (name.empty()) throw runtime_error("Missing 'name' in JSON payload.");
            
            output_json = system.addUser(name);
            exit_code = 0;
        }
        else if (command == "--add" && argc == 4 && string(argv[2]) == "product") {
            // --add product '{"name": "X", "category": "Y", "price": 99.99}'
            string raw_json = argv[3];
            string name = extractJsonValue(raw_json, "name");
            string category = extractJsonValue(raw_json, "category");
            double price = stod(extractJsonValue(raw_json, "price"));

            if (name.empty() || category.empty()) throw runtime_error("Missing 'name' or 'category' in JSON payload.");
            
            output_json = system.addProduct(name, category, price);
            exit_code = 0;
        }
        else if (command == "--add" && argc == 4 && string(argv[2]) == "review") {
            // --add review '{"user_id": 101, "product_id": 1001, "rating": 5, "comment": "Great!"}'
            string raw_json = argv[3];
            int userId = stoi(extractJsonValue(raw_json, "user_id"));
            int productId = stoi(extractJsonValue(raw_json, "product_id"));
            int rating = stoi(extractJsonValue(raw_json, "rating"));
            string comment = extractJsonValue(raw_json, "comment");

            output_json = system.addReview(userId, productId, rating, comment);
            exit_code = 0;
        }
        
    } catch (const std::exception& e) {
        output_json = "{\"error\": \"Processing failed: Invalid argument format or internal error.\", \"details\":\"" + escapeJsonString(e.what()) + "\"}";
        exit_code = 1; 
    } catch (...) {
        output_json = "{\"error\": \"An unknown internal C++ error occurred.\"}";
        exit_code = 1;
    }

    // Print the final JSON output to stdout
    cout << output_json << endl;
    return exit_code;
}
