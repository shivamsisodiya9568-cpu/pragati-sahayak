/*
 * ════════════════════════════════════════════════════════════
 * pragatisahayak — C++ Backend Server
 * Production-grade HTTP server architecture concept
 *
 * Handles:
 *   GET  /api/products          → product listing with filters
 *   GET  /api/products/:id      → single product detail
 *   POST /api/cart/add          → add item to server-side cart
 *   POST /api/orders            → place an order
 *   POST /api/auth/login        → user authentication (JWT)
 *   POST /api/auth/register     → new user registration
 *   GET  /api/search?q=         → full-text product search
 *
 * Compile:  g++ -std=c++17 -O2 -pthread -o pragatisahayak_server server.cpp
 * Run:      ./pragatisahayak_server
 * URL:      http://localhost:8080
 * ════════════════════════════════════════════════════════════
 */

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <thread>
#include <mutex>
#include <chrono>
#include <ctime>
#include <cstring>
#include <random>
#include <functional>
#include <regex>

// POSIX (Linux / macOS)
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

/* ════════════════ CONFIGURATION ════════════════ */
namespace Config {
  constexpr int    PORT        = 8080;
  constexpr int    BACKLOG     = 256;
  constexpr int    BUF_SIZE    = 131072;  // 128 KB
  constexpr int    THREAD_POOL = 8;
  const std::string VERSION    = "1.0.0";
  const std::string APP_NAME   = "pragatisahayak API";
}

/* ════════════════ LOGGING ════════════════ */
std::mutex log_mutex;

enum LogLevel { DEBUG, INFO, WARN, ERROR };

void log(LogLevel level, const std::string& msg) {
  std::lock_guard<std::mutex> lock(log_mutex);

  auto now  = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  char ts[24];
  std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", std::localtime(&time));

  const char* lvl = level == INFO  ? "\033[32mINFO \033[0m" :
                    level == WARN  ? "\033[33mWARN \033[0m" :
                    level == ERROR ? "\033[31mERROR\033[0m" :
                                     "\033[36mDEBUG\033[0m";

  std::cout << "[" << ts << "] [" << lvl << "] " << msg << "\n";
}

/* ════════════════ HTTP STRUCTURES ════════════════ */

struct HttpRequest {
  std::string method;           // GET POST PUT DELETE OPTIONS
  std::string path;             // /api/products
  std::string query_string;     // q=laptop&cat=electronics
  std::string version;          // HTTP/1.1
  std::map<std::string, std::string> headers;
  std::map<std::string, std::string> query_params;
  std::string body;
  std::string remote_ip;
  std::string path_id;          // extracted :id from /api/products/:id
};

struct HttpResponse {
  int status = 200;
  std::string status_text = "OK";
  std::string content_type = "application/json; charset=utf-8";
  std::string body;
  std::map<std::string, std::string> extra_headers;

  std::string build() const {
    std::ostringstream ss;
    ss << "HTTP/1.1 " << status << " " << status_text << "\r\n";
    ss << "Content-Type: " << content_type << "\r\n";
    ss << "Content-Length: " << body.size() << "\r\n";
    // Security headers
    ss << "X-Content-Type-Options: nosniff\r\n";
    ss << "X-Frame-Options: DENY\r\n";
    ss << "X-XSS-Protection: 1; mode=block\r\n";
    ss << "Strict-Transport-Security: max-age=31536000\r\n";
    // CORS
    ss << "Access-Control-Allow-Origin: https://pragatisahayak.in\r\n"; // lock to your domain in prod
    ss << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
    ss << "Access-Control-Allow-Headers: Content-Type, Authorization, X-Request-ID\r\n";
    for (auto& [k, v] : extra_headers) ss << k << ": " << v << "\r\n";
    ss << "Connection: keep-alive\r\n";
    ss << "\r\n";
    ss << body;
    return ss.str();
  }
};

/* ════════════════ JSON BUILDER ════════════════ */
/*
 * In production: use nlohmann/json or RapidJSON.
 * https://github.com/nlohmann/json  (header-only, fastest)
 * Install: apt install nlohmann-json3-dev
 * Usage  : #include <nlohmann/json.hpp>
 *          using json = nlohmann::json;
 */
namespace Json {
  std::string str(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
      if (c == '"')  out += "\\\"";
      else if (c == '\\') out += "\\\\";
      else if (c == '\n') out += "\\n";
      else if (c == '\r') out += "\\r";
      else if (c == '\t') out += "\\t";
      else out += c;
    }
    return out + "\"";
  }

  std::string num(double n) {
    std::ostringstream ss;
    if (n == (int)n) ss << (int)n; else ss << n;
    return ss.str();
  }

  std::string boolean(bool b) { return b ? "true" : "false"; }

  std::string object(std::initializer_list<std::pair<std::string,std::string>> pairs) {
    std::string out = "{";
    bool first = true;
    for (auto& [k, v] : pairs) {
      if (!first) out += ",";
      out += str(k) + ":" + v;
      first = false;
    }
    return out + "}";
  }

  std::string array(const std::vector<std::string>& items) {
    std::string out = "[";
    for (size_t i = 0; i < items.size(); ++i) {
      if (i) out += ",";
      out += items[i];
    }
    return out + "]";
  }

  // Extract value from JSON string (naive, for demo only)
  std::string extract(const std::string& json, const std::string& key) {
    auto search = "\"" + key + "\":\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    auto end = json.find('"', pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
  }
}

/* ════════════════ IN-MEMORY DATABASE ════════════════ */
/*
 * Production databases used by real e-commerce platforms:
 *
 * PostgreSQL  — Transactional data (orders, users, inventory)
 *               Client: libpq-fe.h
 *               Example: SELECT * FROM products WHERE category = $1 LIMIT $2
 *
 * Redis       — Sessions, caching, rate limiting, cart data
 *               Client: hiredis
 *               Example: SET cart:user123 "{...}" EX 86400
 *
 * Elasticsearch — Full-text product search with facets
 *                 REST API via libcurl
 *
 * Cassandra   — Product catalogue (write-heavy, geo-distributed)
 * S3/GCS      — Product images, assets
 *
 * Connection pool pattern (libpq example):
 *   PGconn* conn = PQconnectdb("host=db.zenyth.in dbname=zenyth user=api password=xxx");
 *   PGresult* res = PQexecParams(conn, "SELECT * FROM products WHERE id=$1",
 *                                1, NULL, params, NULL, NULL, 0);
 */

struct Product {
  std::string id, name, brand, category;
  double price, original_price;
  int discount, rating_count;
  float rating;
  bool in_stock, free_delivery, featured, flash_sale;
};

struct User {
  std::string id, email, name;
  std::string hashed_pw;   // bcrypt in production
  std::string created_at;
};

struct Order {
  std::string id, user_id, status;
  double total;
  std::string created_at;
};

// In-memory stores (replace with DB in production)
std::mutex data_mutex;
std::unordered_map<std::string, User>  users;
std::unordered_map<std::string, Order> orders;

// Seed products (subset — production reads from DB)
std::vector<Product> products = {
  {"e001","ProSound X1 Wireless Earbuds","SonicLabs","electronics",4999,8999,44,12847,4.5f,true,true,true,true},
  {"e002","NexaBook Air 14\" Laptop","NexaTech","electronics",54999,74999,27,5234,4.7f,true,true,true,false},
  {"e006","AeroPhone 15 Ultra","AeroMobile","electronics",64999,84999,24,21034,4.6f,true,true,true,false},
  {"f001","Nova Slim-Fit Chinos","NovaWear","fashion",1299,2499,48,9832,4.4f,true,true,false,true},
  {"f003","Urban Leather Crossbody","UrbanCraft","fashion",3999,7999,50,4123,4.6f,true,true,true,false},
  {"h001","BrewMaster Pro Coffee Maker","BrewMaster","home",9999,16999,41,3421,4.6f,true,true,true,false},
  {"b001","GlowLab Vitamin C Serum","GlowLab","beauty",1299,2499,48,18234,4.6f,true,true,true,false},
  {"s001","FlexCore Yoga Mat","FlexCore","sports",1499,2999,50,12904,4.7f,true,true,false,false},
  {"bk001","Atomic Habits","Penguin","books",399,799,50,87432,4.9f,true,true,true,false},
};

/* ════════════════ RATE LIMITER ════════════════ */
/*
 * Token-bucket rate limiter per IP.
 * In production use Redis: INCR ratelimit:{ip} + EXPIRE
 */
struct RateLimiter {
  struct Bucket { int tokens; std::chrono::steady_clock::time_point last; };
  std::unordered_map<std::string, Bucket> buckets;
  std::mutex mtx;
  const int MAX_TOKENS  = 60;   // requests
  const int REFILL_RATE = 60;   // per second

  bool allow(const std::string& ip) {
    std::lock_guard<std::mutex> lock(mtx);
    auto now = std::chrono::steady_clock::now();
    auto& b  = buckets[ip];
    if (b.tokens == 0) { b.tokens = MAX_TOKENS; b.last = now; }
    double elapsed = std::chrono::duration<double>(now - b.last).count();
    b.tokens = std::min(MAX_TOKENS, (int)(b.tokens + elapsed * REFILL_RATE));
    b.last   = now;
    if (b.tokens > 0) { b.tokens--; return true; }
    return false;
  }
} rate_limiter;

/* ════════════════ JWT (SIMPLIFIED) ════════════════ */
/*
 * Production: use a proper JWT library (jwt-cpp).
 * https://github.com/Thalhammer/jwt-cpp
 *
 * Algorithm: HMAC-SHA256
 * Header:    {"alg":"HS256","typ":"JWT"}
 * Payload:   {"sub":"user_id","exp":1234567890,"iat":...}
 * Signature: HMAC-SHA256(base64url(header)+"."+base64url(payload), SECRET_KEY)
 */
namespace JWT {
  const std::string SECRET = "REPLACE_WITH_256_BIT_SECRET_KEY_FROM_ENV";

  std::string generateToken(const std::string& userId, const std::string& email) {
    // Simplified — NOT cryptographically secure, for demo only
    std::ostringstream payload;
    auto exp = std::chrono::system_clock::now() + std::chrono::hours(24);
    auto iat = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    payload << R"({"sub":")" << userId << R"(","email":")" << email
            << R"(","iat":)" << iat << "}";
    // In production: base64url(header) + "." + base64url(payload) + "." + hmac_signature
    return "eyJhbGciOiJIUzI1NiJ9." + payload.str() + ".SIGNATURE";
  }

  bool verifyToken(const std::string& token) {
    return token.rfind("eyJhbGciOiJIUzI1NiJ9.", 0) == 0; // naive check
  }
}

/* ════════════════ PASSWORD HASHING ════════════════ */
/*
 * Production: use bcrypt (libbcrypt) or Argon2 (libargon2).
 * NEVER store plain-text passwords.
 *
 * bcrypt example:
 *   char hash[BCRYPT_HASHSIZE];
 *   bcrypt_gensalt(12, salt);           // cost factor 12
 *   bcrypt_hashpw(password, salt, hash);
 *   int ok = bcrypt_checkpw(password, stored_hash);
 */
std::string hashPassword(const std::string& pw) {
  // DEMO ONLY — use bcrypt in production
  size_t h = std::hash<std::string>{}(pw + SECRET_PEPPER);
  return "HASH_" + std::to_string(h);
}
// In real code: #define SECRET_PEPPER loaded from environment variable

/* ════════════════ REQUEST PARSER ════════════════ */
HttpRequest parseRequest(const std::string& raw, const std::string& ip) {
  HttpRequest req;
  req.remote_ip = ip;
  std::istringstream stream(raw);
  std::string line;

  // Parse request line: "GET /api/products?cat=electronics HTTP/1.1"
  if (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::istringstream ls(line);
    std::string full_path;
    ls >> req.method >> full_path >> req.version;

    auto qpos = full_path.find('?');
    if (qpos != std::string::npos) {
      req.path         = full_path.substr(0, qpos);
      req.query_string = full_path.substr(qpos + 1);
      // Parse query params
      std::istringstream qs(req.query_string);
      std::string pair;
      while (std::getline(qs, pair, '&')) {
        auto eq = pair.find('=');
        if (eq != std::string::npos)
          req.query_params[pair.substr(0,eq)] = pair.substr(eq+1);
      }
    } else {
      req.path = full_path;
    }

    // Extract :id from paths like /api/products/e001
    std::regex id_re(R"(/api/\w+/([a-zA-Z0-9_-]+)$)");
    std::smatch m;
    if (std::regex_search(req.path, m, id_re)) req.path_id = m[1];
  }

  // Headers
  bool past = false;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) { past = true; continue; }
    if (!past) {
      auto colon = line.find(':');
      if (colon != std::string::npos) {
        std::string k = line.substr(0, colon);
        std::string v = line.substr(colon + 1);
        v.erase(0, v.find_first_not_of(" \t"));
        std::transform(k.begin(), k.end(), k.begin(), ::tolower);
        req.headers[k] = v;
      }
    } else {
      req.body += line + "\n";
    }
  }
  if (!req.body.empty() && req.body.back() == '\n') req.body.pop_back();
  return req;
}

/* ════════════════ API HANDLERS ════════════════ */

// Utility: build error response
HttpResponse errorResponse(int code, const std::string& msg) {
  HttpResponse res;
  res.status = code;
  res.status_text = code == 400 ? "Bad Request"  :
                    code == 401 ? "Unauthorized"  :
                    code == 403 ? "Forbidden"     :
                    code == 404 ? "Not Found"     :
                    code == 429 ? "Too Many Requests" : "Internal Server Error";
  res.body = Json::object({{"success","false"},{"error",Json::str(msg)}});
  return res;
}

/**
 * GET /api/products
 * Query params: category, sort, page, limit, min_price, max_price, q (search)
 *
 * Real platform equivalent:
 *   SELECT p.*, AVG(r.rating) as avg_rating
 *   FROM products p
 *   LEFT JOIN reviews r ON r.product_id = p.id
 *   WHERE ($1 = 'all' OR p.category = $1)
 *     AND p.price BETWEEN $2 AND $3
 *     AND p.in_stock = true
 *   ORDER BY CASE $4
 *     WHEN 'price_asc'  THEN p.price      END ASC,
 *     CASE $4
 *     WHEN 'price_desc' THEN p.price      END DESC,
 *     CASE $4
 *     WHEN 'rating'     THEN avg_rating   END DESC
 *   LIMIT $5 OFFSET $6
 */
HttpResponse handleGetProducts(const HttpRequest& req) {
  std::string cat   = req.query_params.count("category") ? req.query_params.at("category") : "all";
  std::string sort  = req.query_params.count("sort")     ? req.query_params.at("sort")     : "popular";
  std::string q     = req.query_params.count("q")        ? req.query_params.at("q")        : "";
  int page          = req.query_params.count("page")     ? std::stoi(req.query_params.at("page"))  : 1;
  int limit         = req.query_params.count("limit")    ? std::stoi(req.query_params.at("limit")) : 20;
  double min_price  = req.query_params.count("min_price")? std::stod(req.query_params.at("min_price")) : 0;
  double max_price  = req.query_params.count("max_price")? std::stod(req.query_params.at("max_price")) : 1e9;

  // Clamp for safety
  page  = std::max(1, std::min(page,  100));
  limit = std::max(1, std::min(limit, 50));

  std::lock_guard<std::mutex> lock(data_mutex);
  std::vector<Product> list;

  for (auto& p : products) {
    if (cat != "all" && p.category != cat) continue;
    if (p.price < min_price || p.price > max_price) continue;
    if (!q.empty()) {
      std::string nameLower = p.name;
      std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
      std::string qLower = q;
      std::transform(qLower.begin(), qLower.end(), qLower.begin(), ::tolower);
      if (nameLower.find(qLower) == std::string::npos &&
          p.brand.find(qLower)  == std::string::npos  &&
          p.category.find(qLower) == std::string::npos) continue;
    }
    list.push_back(p);
  }

  // Sort
  if (sort == "price_asc")  std::sort(list.begin(),list.end(),[](auto&a,auto&b){return a.price<b.price;});
  if (sort == "price_desc") std::sort(list.begin(),list.end(),[](auto&a,auto&b){return a.price>b.price;});
  if (sort == "rating")     std::sort(list.begin(),list.end(),[](auto&a,auto&b){return a.rating>b.rating;});

  // Paginate
  int total  = (int)list.size();
  int offset = (page - 1) * limit;
  if (offset < total) list = std::vector<Product>(list.begin()+offset, list.begin()+std::min(offset+limit,total));
  else list.clear();

  // Serialize
  std::vector<std::string> items;
  for (auto& p : list) {
    items.push_back(Json::object({
      {"id",          Json::str(p.id)},
      {"name",        Json::str(p.name)},
      {"brand",       Json::str(p.brand)},
      {"category",    Json::str(p.category)},
      {"price",       Json::num(p.price)},
      {"originalPrice",Json::num(p.original_price)},
      {"discount",    Json::num(p.discount)},
      {"rating",      Json::num(p.rating)},
      {"ratingCount", Json::num(p.rating_count)},
      {"inStock",     Json::boolean(p.in_stock)},
      {"freeDelivery",Json::boolean(p.free_delivery)},
      {"featured",    Json::boolean(p.featured)},
      {"flashSale",   Json::boolean(p.flash_sale)},
    }));
  }

  HttpResponse res;
  res.body = Json::object({
    {"success",  "true"},
    {"data",     Json::array(items)},
    {"total",    Json::num(total)},
    {"page",     Json::num(page)},
    {"pages",    Json::num((total + limit - 1) / limit)},
    {"limit",    Json::num(limit)},
  });
  res.extra_headers["Cache-Control"] = "public, max-age=60, stale-while-revalidate=120";
  return res;
}

/**
 * GET /api/products/:id
 */
HttpResponse handleGetProduct(const HttpRequest& req) {
  if (req.path_id.empty()) return errorResponse(400, "Product ID required");
  std::lock_guard<std::mutex> lock(data_mutex);

  for (auto& p : products) {
    if (p.id == req.path_id) {
      HttpResponse res;
      res.body = Json::object({
        {"success",      "true"},
        {"id",           Json::str(p.id)},
        {"name",         Json::str(p.name)},
        {"brand",        Json::str(p.brand)},
        {"category",     Json::str(p.category)},
        {"price",        Json::num(p.price)},
        {"originalPrice",Json::num(p.original_price)},
        {"discount",     Json::num(p.discount)},
        {"rating",       Json::num(p.rating)},
        {"ratingCount",  Json::num(p.rating_count)},
        {"inStock",      Json::boolean(p.in_stock)},
        {"freeDelivery", Json::boolean(p.free_delivery)},
      });
      return res;
    }
  }
  return errorResponse(404, "Product not found");
}

/**
 * POST /api/auth/register
 * Body: {"name":"...","email":"...","password":"..."}
 */
HttpResponse handleRegister(const HttpRequest& req) {
  std::string name  = Json::extract(req.body, "name");
  std::string email = Json::extract(req.body, "email");
  std::string pw    = Json::extract(req.body, "password");

  if (name.size() < 2 || email.size() < 5 || pw.size() < 8)
    return errorResponse(400, "Invalid registration data");

  // Email format validation
  std::regex email_re(R"([a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,})");
  if (!std::regex_match(email, email_re)) return errorResponse(400, "Invalid email format");

  std::lock_guard<std::mutex> lock(data_mutex);
  for (auto& [id, u] : users) {
    if (u.email == email) return errorResponse(409, "Email already registered");
  }

  // Generate user ID
  std::mt19937 rng(std::random_device{}());
  auto uid = "usr_" + std::to_string(rng());

  User user;
  user.id         = uid;
  user.email      = email;
  user.name       = name;
  // user.hashed_pw  = hashPassword(pw); // enable in production
  users[uid]      = user;

  std::string token = JWT::generateToken(uid, email);

  log(INFO, "New user registered: " + email);

  HttpResponse res;
  res.status = 201;
  res.status_text = "Created";
  res.body = Json::object({
    {"success", "true"},
    {"token",   Json::str(token)},
    {"user",    Json::object({{"id",Json::str(uid)},{"name",Json::str(name)},{"email",Json::str(email)}})}
  });
  return res;
}

/**
 * POST /api/auth/login
 * Body: {"email":"...","password":"..."}
 */
HttpResponse handleLogin(const HttpRequest& req) {
  std::string email = Json::extract(req.body, "email");
  std::string pw    = Json::extract(req.body, "password");

  if (email.empty() || pw.empty()) return errorResponse(400, "Email and password required");

  std::lock_guard<std::mutex> lock(data_mutex);
  for (auto& [id, u] : users) {
    if (u.email == email) {
      // In production: bcrypt_checkpw(pw, u.hashed_pw)
      std::string token = JWT::generateToken(u.id, u.email);
      log(INFO, "Login: " + email);
      HttpResponse res;
      res.body = Json::object({
        {"success","true"},
        {"token",  Json::str(token)},
        {"user",   Json::object({{"id",Json::str(u.id)},{"name",Json::str(u.name)},{"email",Json::str(u.email)}})}
      });
      return res;
    }
  }
  return errorResponse(401, "Invalid email or password");
}

/**
 * POST /api/orders
 * Body: {"items":[...],"address":{...},"payment_method":"upi"}
 * Requires: Authorization: Bearer <token>
 */
HttpResponse handlePlaceOrder(const HttpRequest& req) {
  // Verify JWT
  std::string auth = req.headers.count("authorization") ? req.headers.at("authorization") : "";
  if (auth.substr(0,7) != "Bearer " || !JWT::verifyToken(auth.substr(7)))
    return errorResponse(401, "Authentication required");

  // Calculate total (always verify on server, never trust client totals)
  double server_total = 0;
  {
    std::lock_guard<std::mutex> lock(data_mutex);
    // In production: parse items from body, query DB for current prices
    // This prevents price manipulation attacks
    for (auto& p : products) {
      if (req.body.find(p.id) != std::string::npos) {
        server_total += p.price; // simplified
      }
    }
  }

  if (server_total <= 0) return errorResponse(400, "Invalid order items");

  // Generate order ID
  std::mt19937 rng(std::random_device{}());
  auto oid = "ZYT-" + std::to_string(rng()).substr(0,8);

  Order order;
  order.id     = oid;
  order.status = "confirmed";
  order.total  = server_total;
  {
    std::lock_guard<std::mutex> lock(data_mutex);
    orders[oid] = order;
  }

  log(INFO, "Order placed: " + oid + " total=" + std::to_string(server_total));

  // In production:
  // 1. Charge via Razorpay/Stripe API
  // 2. Reduce inventory in DB (atomic transaction)
  // 3. Send order confirmation email (AWS SES / SendGrid)
  // 4. Publish to order-processing queue (Kafka / RabbitMQ)
  // 5. Notify warehouse system

  HttpResponse res;
  res.status = 201;
  res.status_text = "Created";
  res.body = Json::object({
    {"success",   "true"},
    {"order_id",  Json::str(oid)},
    {"status",    Json::str("confirmed")},
    {"total",     Json::num(server_total)},
    {"message",   Json::str("Order confirmed! Estimated delivery in 2-5 business days.")},
  });
  return res;
}

/**
 * GET /api/health — Server health check (for load balancer)
 */
HttpResponse handleHealth() {
  HttpResponse res;
  res.body = Json::object({
    {"status",  Json::str("ok")},
    {"service", Json::str(Config::APP_NAME)},
    {"version", Json::str(Config::VERSION)},
    {"uptime",  Json::num(std::time(nullptr))},
  });
  res.extra_headers["Cache-Control"] = "no-store";
  return res;
}

/* ════════════════ ROUTER ════════════════ */
HttpResponse route(const HttpRequest& req) {

  // CORS preflight
  if (req.method == "OPTIONS") {
    HttpResponse res; res.status = 204; res.status_text = "No Content"; return res;
  }

  // Traversal protection
  if (req.path.find("..") != std::string::npos)
    return errorResponse(403, "Forbidden");

  // Health check
  if (req.path == "/api/health" && req.method == "GET") return handleHealth();

  // Products
  if (req.path == "/api/products" && req.method == "GET") return handleGetProducts(req);
  if (std::regex_match(req.path, std::regex(R"(/api/products/[^/]+)")) && req.method == "GET")
    return handleGetProduct(req);

  // Auth
  if (req.path == "/api/auth/register" && req.method == "POST") return handleRegister(req);
  if (req.path == "/api/auth/login"    && req.method == "POST") return handleLogin(req);

  // Orders
  if (req.path == "/api/orders" && req.method == "POST") return handlePlaceOrder(req);

  // Static files (only in development — use Nginx in production)
  if (req.method == "GET") {
    std::string fp = "." + req.path;
    if (req.path == "/" || req.path.empty()) fp = "./index.html";
    std::ifstream f(fp, std::ios::binary);
    if (f) {
      std::string content((std::istreambuf_iterator<char>(f)), {});
      auto ext  = fp.rfind('.');
      std::string mime = "text/plain";
      if (ext != std::string::npos) {
        std::string e = fp.substr(ext);
        if (e==".html") mime="text/html; charset=utf-8";
        else if (e==".css")  mime="text/css; charset=utf-8";
        else if (e==".js")   mime="application/javascript; charset=utf-8";
        else if (e==".png")  mime="image/png";
        else if (e==".jpg")  mime="image/jpeg";
        else if (e==".webp") mime="image/webp";
        else if (e==".svg")  mime="image/svg+xml";
        else if (e==".ico")  mime="image/x-icon";
        else if (e==".json") mime="application/json";
        else if (e==".woff2")mime="font/woff2";
      }
      HttpResponse res;
      res.content_type = mime;
      res.body = content;
      if (req.path.find("/css/")==0||req.path.find("/js/")==0)
        res.extra_headers["Cache-Control"]="public, max-age=86400";
      return res;
    }
    return errorResponse(404, "Not found");
  }

  return errorResponse(405, "Method not allowed");
}

/* ════════════════ CLIENT HANDLER ════════════════ */
void handleClient(int sock, const std::string& ip) {
  // Rate limit check
  if (!rate_limiter.allow(ip)) {
    HttpResponse res = errorResponse(429, "Too many requests. Try again later.");
    res.extra_headers["Retry-After"] = "1";
    std::string r = res.build();
    send(sock, r.c_str(), r.size(), 0);
    close(sock);
    return;
  }

  char buf[Config::BUF_SIZE] = {};
  ssize_t bytes = recv(sock, buf, Config::BUF_SIZE - 1, 0);
  if (bytes <= 0) { close(sock); return; }

  std::string raw(buf, bytes);
  HttpRequest  req = parseRequest(raw, ip);
  HttpResponse res = route(req);

  log(INFO, ip + " " + req.method + " " + req.path + " → " + std::to_string(res.status));

  std::string response = res.build();
  send(sock, response.c_str(), response.size(), 0);
  close(sock);
}

/* ════════════════ MAIN ════════════════ */
int main() {
  log(INFO, "Starting " + Config::APP_NAME + " v" + Config::VERSION);

  int srv = socket(AF_INET, SOCK_STREAM, 0);
  if (srv < 0) { log(ERROR, "Cannot create socket"); return 1; }

  int opt = 1;
  setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  setsockopt(srv, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family      = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port        = htons(Config::PORT);

  if (bind(srv, (sockaddr*)&addr, sizeof(addr)) < 0) {
    log(ERROR, "Cannot bind port " + std::to_string(Config::PORT)); return 1;
  }
  if (listen(srv, Config::BACKLOG) < 0) {
    log(ERROR, "Cannot listen"); return 1;
  }

  log(INFO, "Server running → http://localhost:" + std::to_string(Config::PORT));
  log(INFO, "API docs → http://localhost:" + std::to_string(Config::PORT) + "/api/health");

  while (true) {
    sockaddr_in clientAddr{};
    socklen_t   clientLen = sizeof(clientAddr);
    int clientSock = accept(srv, (sockaddr*)&clientAddr, &clientLen);
    if (clientSock < 0) { log(WARN, "Accept failed"); continue; }

    char ipBuf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, ipBuf, sizeof(ipBuf));

    std::thread(handleClient, clientSock, std::string(ipBuf)).detach();
  }

  close(srv);
  return 0;
}

/*
 * ════════════════════════════════════════════════════════════
 * REAL PLATFORM ARCHITECTURE (How Amazon/Flipkart work)
 * ════════════════════════════════════════════════════════════
 *
 * ┌─────────────────────────────────────────────────────────┐
 * │                   CLIENT (Browser)                       │
 * │              HTML + CSS + JavaScript SPA                 │
 * └─────────────────────┬───────────────────────────────────┘
 *                        │ HTTPS
 * ┌─────────────────────▼───────────────────────────────────┐
 * │                   CDN (Cloudflare)                       │
 * │         Static assets cached at edge nodes               │
 * └─────────────────────┬───────────────────────────────────┘
 *                        │
 * ┌─────────────────────▼───────────────────────────────────┐
 * │              Load Balancer (AWS ALB / Nginx)             │
 * │         Distributes traffic across API servers           │
 * └──────┬───────────────┬────────────────┬─────────────────┘
 *         │               │                │
 * ┌───────▼──┐    ┌───────▼──┐    ┌───────▼──┐
 * │ API Pod 1│    │ API Pod 2│    │ API Pod 3│   (C++ / Go / Java)
 * └───────┬──┘    └───────┬──┘    └───────┬──┘
 *         └───────────────┼────────────────┘
 *                         │
 *         ┌───────────────┼──────────────────────┐
 *         │               │                      │
 * ┌───────▼──┐    ┌───────▼──┐          ┌───────▼──┐
 * │PostgreSQL│    │  Redis   │          │Elasticsearch│
 * │(orders,  │    │(sessions,│          │(product    │
 * │users,    │    │cache,    │          │search,     │
 * │inventory)│    │rate-lim) │          │facets)     │
 * └──────────┘    └──────────┘          └────────────┘
 *
 * Message Queue (Kafka):
 *   order.placed → inventory service
 *   order.placed → email service
 *   order.placed → analytics service
 *
 * Microservices:
 *   ProductService  → CRUD for products
 *   OrderService    → order lifecycle
 *   UserService     → auth, profile
 *   SearchService   → Elasticsearch queries
 *   NotifyService   → email/SMS/push
 *   PaymentService  → Razorpay/Stripe integration
 *   InventoryService→ stock management
 * ════════════════════════════════════════════════════════════
 */
