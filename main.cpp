#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <sstream> 
#include <stdexcept> 
#include <limits> 
#include <numeric> // Required for std::accumulate

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
        if (end != string::npos) {
            string val = json.substr(start, end - start);
            
            // Unescape the string content if needed (simplified)
            size_t pos = val.find("\\\"");
            while(pos != string::npos) {
                val.replace(pos, 2, "\"");
                pos = val.find("\\\"", pos + 1);
            }
            return val;
        }
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

    /**
     * Attempts to create initial default data if no files exist.
     */
    void createDefaultData() {
        products.emplace_back(1000, "Mechanical Keyboard", "Electronics", 99.99);
        products.emplace_back(1001, "Wireless Mouse", "Electronics", 45.50);
        products.emplace_back(1002, "The Silent Patient Book", "Books", 12.00);
        products.emplace_back(1003, "Blue Hoodie", "Apparel", 65.00);
        nextProductId = 1004;

        users.emplace_back(100, "Alice Johnson");
        users.emplace_back(101, "Bob Smith");
        nextUserId = 102;

        reviews.emplace_back(100, 1000, 5, "Excellent keyboard for coding.");
        reviews.emplace_back(100, 1001, 4, "Reliable mouse, good battery life.");
        reviews.emplace_back(101, 1002, 3, "A decent thriller, a bit slow.");
        reviews.emplace_back(101, 1003, 5, "Comfy and warm!");
    }


    // --- Persistence Methods ---

    /** Reads all data from JSON files into memory. */
    void loadData() {
        products.clear();
        users.clear();
        reviews.clear();
        
        // Helper to read all lines from a file
        auto readAll = [](const string& filename) {
            ifstream ifs(filename);
            if (!ifs.is_open()) {
                cerr << "DEBUG C++: File not found or failed to open: " << filename << endl; 
                return string("[]"); 
            }
            if (ifs.peek() == ifstream::traits_type::eof()) {
                return string("[]");
            }
            return string((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
        };
        
        // Simple JSON array to vector parser
        auto parseArray = [](const string& raw_json) {
            vector<string> items;
            if (raw_json.empty() || raw_json.size() < 2 || raw_json[0] != '[' || raw_json.back() != ']') {
                return items;
            }
            string content = raw_json.substr(1, raw_json.size() - 2); 
            
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
            
            for(string& item : items) {
                 size_t first = item.find_first_not_of(" \t\n\r");
                 size_t last = item.find_last_not_of(" \t\n\r");
                 if (string::npos != first) {
                     item = item.substr(first, (last - first + 1));
                 } else {
                     item = ""; 
                 }
            }
             items.erase(std::remove_if(items.begin(), items.end(), [](const std::string& s){ return s.empty() || s.find('{') == std::string::npos; }), items.end());
            return items;
        };
        
        bool data_loaded = false;

        // 1. Load Products
        for (const auto& raw_obj : parseArray(readAll(PRODUCTS_FILE))) {
            try {
                int id = stoi(extractJsonValue(raw_obj, "id"));
                double price = stod(extractJsonValue(raw_obj, "price"));
                products.emplace_back(id, extractJsonValue(raw_obj, "name"), 
                                     extractJsonValue(raw_obj, "category"), price);
                nextProductId = max(nextProductId, id + 1);
                data_loaded = true;
            } catch (...) {}
        }
        
        // 2. Load Users
        for (const auto& raw_obj : parseArray(readAll(USERS_FILE))) {
            try {
                int id = stoi(extractJsonValue(raw_obj, "id"));
                users.emplace_back(id, extractJsonValue(raw_obj, "name"));
                nextUserId = max(nextUserId, id + 1);
                data_loaded = true;
            } catch (...) {}
        }

        // 3. Load Reviews
        for (const auto& raw_obj : parseArray(readAll(REVIEWS_FILE))) {
            try {
                int uid = stoi(extractJsonValue(raw_obj, "user_id"));
                int pid = stoi(extractJsonValue(raw_obj, "product_id"));
                int rating = stoi(extractJsonValue(raw_obj, "rating"));
                reviews.emplace_back(uid, pid, rating, extractJsonValue(raw_obj, "comment"));
                data_loaded = true;
            } catch (...) {}
        }
        
        // If no persistent data was found, create the default set and save it
        if (!data_loaded) {
            createDefaultData();
            saveData(); 
        }
    }

    /** Writes all in-memory data back to JSON files. */
    void saveData() const {
        // Helper to write a vector of JSON objects to a file
        auto writeVector = [](const string& filename, const auto& vec) {
            ofstream ofs(filename);
            if (ofs.is_open()) {
                ofs << "[" << endl;
                for (size_t i = 0; i < vec.size(); ++i) {
                    ofs << vec[i].toJson();
                    if (i < vec.size() - 1) ofs << ",";
                    ofs << endl;
                }
                ofs << "]";
            } else {
                 cerr << "DEBUG C++: FAILED to write file: " << filename << endl;
            }
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
    
    // Helper to check if a user has reviewed a product
    bool hasUserReviewed(int userId, int productId) const {
        for (const auto& review : reviews) {
            if (review.getUserId() == userId && review.getProductId() == productId) {
                return true;
            }
        }
        return false;
    }

    // Helper to get all product IDs reviewed by a user
    vector<int> getReviewedProductIds(int userId) const {
        vector<int> reviewed_ids;
        for (const auto& review : reviews) {
            if (review.getUserId() == userId) {
                reviewed_ids.push_back(review.getProductId());
            }
        }
        return reviewed_ids;
    }

public:
    // Constructor (Default)
    RecommendationSystem() {}

    // --- JSON Getters (Read Operations) ---

    string getProductsJson() {
        loadData(); // Load latest state before generating output
        ostringstream oss;
        oss << "{\"products\":[";
        for (size_t i = 0; i < products.size(); ++i) {
            const auto& p = products[i];
            
            string product_json_base = p.toJson();
            product_json_base.pop_back(); 
            
            oss << product_json_base; 
            oss << ", \"avg_rating\":" << fixed << setprecision(2) << calculateAverageRating(p.getId());
            oss << ", \"reviews_count\":" << count_if(reviews.begin(), reviews.end(), 
                                      [&p](const Review& r){ return r.getProductId() == p.getId(); });
            oss << "}";

            if (i < products.size() - 1) oss << ",";
        }
        oss << "]}";
        return oss.str();
    }

    string getUsersJson() {
        loadData(); // Load latest state before generating output
        ostringstream oss;
        oss << "{\"users\":[";
        for (size_t i = 0; i < users.size(); ++i) {
            oss << users[i].toJson();
            if (i < users.size() - 1) oss << ",";
        }
        oss << "]}";
        return oss.str();
    }

    string getReviewsJson(int productId) {
        loadData();
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
        loadData(); 
        
        int newId = nextUserId++;
        users.emplace_back(newId, name);
        saveData(); // Save changes
        
        return "{\"status\":\"success\", \"message\":\"User added successfully.\", \"id\":" + to_string(newId) + ", \"name\":\"" + escapeJsonString(name) + "\"}";
    }

    string addProduct(const string& name, const string& category, double price) {
        loadData();

        int newId = nextProductId++;
        products.emplace_back(newId, name, category, price);
        saveData();

        return "{\"status\":\"success\", \"message\":\"Product added successfully.\", \"id\":" + to_string(newId) + "}";
    }
    
    string purchaseProduct(int userId, int productId) {
        loadData();
        
        if (findUserById(userId) == nullptr) return "{\"status\":\"error\", \"message\":\"User not found.\"}";
        if (findProductById(productId) == nullptr) return "{\"status\":\"error\", \"message\":\"Product not found.\"}";
        
        // NOTE: In the persistent model, 'purchase' is just a status update and is handled 
        // by the C++ engine confirming the items exist.
        
        return "{\"status\":\"success\", \"message\":\"Purchase recorded (no dedicated purchase history storage in this C++ version).\"}";
    }

    string rateProduct(int userId, int productId, int rating) {
        // Redirects to addReview as it's the persistent way to track ratings
        return addReview(userId, productId, rating, "No comment provided.");
    }


    string addReview(int userId, int productId, int rating, const string& comment) {
        loadData();

        if (findUserById(userId) == nullptr) return "{\"status\":\"error\", \"message\":\"User not found.\"}";
        if (findProductById(productId) == nullptr) return "{\"status\":\"error\", \"message\":\"Product not found.\"}";
        if (rating < 1 || rating > 5) return "{\"status\":\"error\", \"message\":\"Invalid rating (1-5).\"}";
        
        if (hasUserReviewed(userId, productId)) return "{\"status\":\"error\", \"message\":\"User has already reviewed this product.\"}";


        reviews.emplace_back(userId, productId, rating, comment);
        saveData();

        return "{\"status\":\"success\", \"message\":\"Review added.\", \"product_id\":" + to_string(productId) + ", \"new_avg_rating\":" + to_string(calculateAverageRating(productId)) + "}";
    }

    string deleteUser(int userId) {
        loadData();
        
        // Find and remove the user
        auto it = find_if(users.begin(), users.end(), 
            [userId](const User& u) { return u.getId() == userId; });
        
        if (it == users.end()) {
            return "{\"status\":\"error\", \"message\":\"User not found.\"}";
        }
        
        users.erase(it);
        
        // Also remove all reviews by this user
        reviews.erase(
            remove_if(reviews.begin(), reviews.end(),
                [userId](const Review& r) { return r.getUserId() == userId; }),
            reviews.end()
        );
        
        saveData();
        return "{\"status\":\"success\", \"message\":\"User deleted successfully.\", \"id\":" + to_string(userId) + "}";
    }

    string deleteProduct(int productId) {
        loadData();
        
        // Find and remove the product
        auto it = find_if(products.begin(), products.end(), 
            [productId](const Product& p) { return p.getId() == productId; });
        
        if (it == products.end()) {
            return "{\"status\":\"error\", \"message\":\"Product not found.\"}";
        }
        
        products.erase(it);
        
        // Also remove all reviews for this product
        reviews.erase(
            remove_if(reviews.begin(), reviews.end(),
                [productId](const Review& r) { return r.getProductId() == productId; }),
            reviews.end()
        );
        
        saveData();
        return "{\"status\":\"success\", \"message\":\"Product deleted successfully.\", \"id\":" + to_string(productId) + "}";
    }
    
    string getRecommendationsJson(int userId) {
        loadData();
        
        const User* user = findUserById(userId);
        if (!user) { return "{\"status\":\"error\", \"message\":\"User not found.\"}"; }

        // 1. Find the category of the last reviewed product
        const vector<int> reviewedIds = getReviewedProductIds(userId);
        if (reviewedIds.empty()) { 
            return "{\"status\":\"error\", \"message\":\"User has no review history for recommendations.\"}";
        }

        int lastReviewedId = reviewedIds.back();
        const Product* lastProduct = findProductById(lastReviewedId);
        if (!lastProduct) { return "{\"status\":\"error\", \"message\":\"Internal data error: Last reviewed product missing.\"}"; }

        const string& targetCategory = lastProduct->getCategory();

        // 2. Filter and collect relevant products (same category, not reviewed)
        vector<pair<double, const Product*>> candidates; // pair of (rating, product*)
        for (const auto& product : products) {
            // Must match category AND not be already reviewed by the user
            if (product.getCategory() == targetCategory && !hasUserReviewed(userId, product.getId())) {
                candidates.push_back({calculateAverageRating(product.getId()), &product});
            }
        }

        if (candidates.empty()) {
             return "{\"status\":\"success\", \"user_id\":" + to_string(userId) + ", \"recommendations\":[], \"message\":\"No new recommendations available in category " + targetCategory + ".\"}";
        }

        // 3. Sort candidates by average rating (descending)
        sort(candidates.begin(), candidates.end(), [](const pair<double, const Product*>& a, const pair<double, const Product*>& b) {
            return a.first > b.first;
        });

        // 4. Build JSON for top 3 recommendations
        ostringstream oss;
        oss << "{"
            << "\"status\":\"success\","
            << "\"user_id\":" << userId << ","
            << "\"target_category\":\"" << targetCategory << "\","
            << "\"recommendations\":[";
        
        int count = 0;
        for (const auto& candidate : candidates) {
            if (count < 3) {
                // Manually construct JSON to include the rating
                string product_json_base = candidate.second->toJson();
                product_json_base.pop_back(); 
                
                oss << product_json_base; 
                oss << ", \"avg_rating\":" << fixed << setprecision(2) << candidate.first;
                oss << ", \"reviews_count\":" << count_if(reviews.begin(), reviews.end(), 
                                      [&candidate](const Review& r){ return r.getProductId() == candidate.second->getId(); });
                oss << "}";

                if (count < 2 && count < candidates.size() - 1) oss << ",";
                count++;
            } else {
                break;
            }
        }
        oss << "]}";
        return oss.str();
    }
};

// --- MAIN ENTRY POINT ---
int main(int argc, char* argv[]) {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

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
        else if (command == "--add-user" && argc == 3) {
            // --add-user <name>
            output_json = system.addUser(argv[2]);
            exit_code = 0;
        }
        else if (command == "--add-product" && argc == 5) {
            // --add-product <name> <category> <price>
            output_json = system.addProduct(argv[2], argv[3], stod(argv[4]));
            exit_code = 0;
        }
        else if (command == "--purchase" && argc == 4) {
            // --purchase <userId> <productId>
            output_json = system.purchaseProduct(stoi(argv[2]), stoi(argv[3]));
            exit_code = 0;
        }
        else if (command == "--rate" && argc == 5) {
            // --rate <userId> <productId> <rating>
            output_json = system.rateProduct(stoi(argv[2]), stoi(argv[3]), stoi(argv[4]));
            exit_code = 0;
        }
        else if (command == "--delete-user" && argc == 3) {
            // --delete-user <userId>
            output_json = system.deleteUser(stoi(argv[2]));
            exit_code = 0;
        }
        else if (command == "--delete-product" && argc == 3) {
            // --delete-product <productId>
            output_json = system.deleteProduct(stoi(argv[2]));
            exit_code = 0;
        }
        else if (command == "--add-review" && argc == 6) {
            // --add-review <userId> <productId> <rating> <comment>
            output_json = system.addReview(stoi(argv[2]), stoi(argv[3]), stoi(argv[4]), argv[5]);
            exit_code = 0;
        }
        else if (command == "--recommend" && argc == 3) {
            // --recommend <userId>
            output_json = system.getRecommendationsJson(stoi(argv[2]));
            exit_code = 0;
        }
        else {
             // Handle the complex --add command structure for flexibility
             if (command == "--add" && argc >= 4) {
                 string resource = argv[2];
                 if (resource == "user" && argc == 4) {
                     // --add user '{"name": "New User"}'
                     string raw_json = argv[3];
                     string name = extractJsonValue(raw_json, "name");
                     if (!name.empty()) {
                        output_json = system.addUser(name); exit_code = 0;
                     }
                 } else if (resource == "product" && argc == 4) {
                      // --add product '{"name": "X", "category": "Y", "price": 99.99}'
                      string raw_json = argv[3];
                      string name = extractJsonValue(raw_json, "name");
                      string category = extractJsonValue(raw_json, "category");
                      double price = stod(extractJsonValue(raw_json, "price"));
                      if (!name.empty() && !category.empty()) {
                          output_json = system.addProduct(name, category, price); exit_code = 0;
                      }
                 } else if (resource == "review" && argc == 4) {
                      // --add review '{"user_id": 101, "product_id": 1001, "rating": 5, "comment": "Great!"}'
                      string raw_json = argv[3];
                      int userId = stoi(extractJsonValue(raw_json, "user_id"));
                      int productId = stoi(extractJsonValue(raw_json, "product_id"));
                      int rating = stoi(extractJsonValue(raw_json, "rating"));
                      string comment = extractJsonValue(raw_json, "comment");
                      output_json = system.addReview(userId, productId, rating, comment); exit_code = 0;
                 }
                 
             }
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
