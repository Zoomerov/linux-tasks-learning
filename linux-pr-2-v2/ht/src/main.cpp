#include <windows.h>
#include <winhttp.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Options {
    std::string package, repository, output;
    std::string format = "svg";
    int depth = -1;
};

static std::string lower(std::string s) {
    for (char &c: s) c = char(std::tolower((unsigned char) c));
    return s;
}

static std::string trim(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

static void usage(std::ostream &out) {
    out << "Usage: nuget-graph --package <id> --repository <URL|path> --output <file> [--format dot|svg] [--depth N]\n";
}

static Options parseArgs(int argc, char **argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") {
            usage(std::cout);
            std::exit(0);
        }
        if (a != "--package" && a != "--repository" && a != "--output" && a != "--format" && a != "--depth")
            throw std::runtime_error("Unknown option: " + a);
        if (++i >= argc) throw std::runtime_error("Missing value for " + a);
        std::string v = argv[i];
        if (a == "--package") o.package = v;
        else if (a == "--repository") o.repository = v;
        else if (a == "--output") o.output = v;
        else if (a == "--format") o.format = lower(v);
        else {
            try {
                size_t p;
                o.depth = std::stoi(v, &p);
                if (p != v.size() || o.depth < 0) throw std::exception();
            } catch (...) { throw std::runtime_error("--depth must be a non-negative integer"); }
        }
    }
    if (o.package.empty() || o.repository.empty() || o.output.empty()) throw std::runtime_error(
        "--package, --repository and --output are required");
    if (o.format != "dot" && o.format != "svg") throw std::runtime_error("--format must be dot or svg");
    if (o.depth < 0) o.depth = 100; // finite safeguard for malformed remote repositories
    return o;
}

static std::string wideToUtf8(const std::wstring &v) {
    if (v.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, v.data(), (int) v.size(), nullptr, 0, nullptr, nullptr);
    std::string r(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, v.data(), (int) v.size(), r.data(), n, nullptr, nullptr);
    return r;
}

static std::string httpGet(const std::string &url) {
    int n = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, w.data(), n);
    w.pop_back();
    URL_COMPONENTS c{};
    c.dwStructSize = sizeof(c);
    c.dwSchemeLength = c.dwHostNameLength = c.dwUrlPathLength = c.dwExtraInfoLength = (DWORD) -1;
    if (!WinHttpCrackUrl(w.c_str(), 0, 0, &c)) throw std::runtime_error("Invalid URL: " + url);
    std::wstring host(c.lpszHostName, c.dwHostNameLength), path(c.lpszUrlPath, c.dwUrlPathLength);
    if (c.dwExtraInfoLength) path.append(c.lpszExtraInfo, c.dwExtraInfoLength);
    HINTERNET s = WinHttpOpen(L"nuget-graph/1.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,
                              WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) throw std::runtime_error("Cannot initialize HTTP client");
    HINTERNET h = WinHttpConnect(s, host.c_str(), c.nPort, 0);
    HINTERNET r = h
                      ? WinHttpOpenRequest(h, L"GET", path.c_str(), nullptr,WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           c.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
                      : nullptr;
    if (!r || !WinHttpSendRequest(r,WINHTTP_NO_ADDITIONAL_HEADERS, 0,WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !
        WinHttpReceiveResponse(r, nullptr)) {
        if (r) WinHttpCloseHandle(r);
        if (h) WinHttpCloseHandle(h);
        WinHttpCloseHandle(s);
        throw std::runtime_error("HTTP request failed: " + url);
    }
    DWORD status = 0, size = sizeof(status);
    WinHttpQueryHeaders(r,WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &size, nullptr);
    std::string body;
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(r, &available) && available) {
        size_t at = body.size();
        body.resize(at + available);
        DWORD got = 0;
        if (!WinHttpReadData(r, body.data() + at, available, &got)) break;
        body.resize(at + got);
    }
    WinHttpCloseHandle(r);
    WinHttpCloseHandle(h);
    WinHttpCloseHandle(s);
    if (status < 200 || status >= 300) throw std::runtime_error("HTTP " + std::to_string(status) + " for " + url);
    return body;
}

// Small JSON scanner: extracts string values of a named property without a JSON library.
static std::vector<std::string> stringsForKey(const std::string &json, const std::string &key) {
    std::vector<std::string> out;
    std::string needle = "\"" + key + "\"";
    size_t p = 0;
    while ((p = json.find(needle, p)) != std::string::npos) {
        p = json.find(':', p + needle.size());
        if (p == std::string::npos) break;
        ++p;
        while (p < json.size() && std::isspace((unsigned char) json[p])) ++p;
        if (p >= json.size() || json[p] != '\"') continue;
        ++p;
        std::string v;
        for (; p < json.size() && json[p] != '\"'; ++p) {
            if (json[p] == '\\' && p + 1 < json.size()) ++p;
            v += json[p];
        }
        out.push_back(v);
        ++p;
    }
    return out;
}

static std::vector<std::string> dependenciesFromRegistration(const std::string &json) {
    std::set<std::string> ids;
    size_t p = 0;
    while ((p = json.find("\"dependencies\"", p)) != std::string::npos) {
        size_t start = json.find('[', p), end = start, level = 0;
        if (start == std::string::npos) break;
        for (; end < json.size(); ++end) {
            if (json[end] == '[') ++level;
            else if (json[end] == ']' && --level == 0) break;
        }
        if (end >= json.size()) break;
        for (auto &id: stringsForKey(json.substr(start, end - start + 1), "id")) if (!id.empty()) ids.insert(id);
        p = end + 1;
    }
    return {ids.begin(), ids.end()};
}

static std::string registrationsUrl(const std::string &serviceIndex) {
    // Resource object contains both @id and @type; select a registrations endpoint (NuGet's standard V3 shape).
    size_t pos = 0;
    while ((pos = serviceIndex.find("RegistrationsBaseUrl", pos)) != std::string::npos) {
        size_t begin = serviceIndex.rfind('{', pos), end = serviceIndex.find('}', pos);
        if (begin == std::string::npos || end == std::string::npos) break;
        auto ids = stringsForKey(serviceIndex.substr(begin, end - begin + 1), "@id");
        if (!ids.empty()) return ids.front();
        ++pos;
    }
    throw std::runtime_error("The service index has no RegistrationsBaseUrl resource");
}

class Repository {
    std::string source_, registrations_;
    bool local_;

public:
    explicit Repository(const std::string &source) : source_(source), local_(fs::is_directory(fs::u8path(source))) {
        if (!local_) registrations_ = registrationsUrl(httpGet(source_));
    }

    std::vector<std::string> deps(const std::string &package) const {
        if (local_) {
            fs::path file = fs::u8path(source_) / (package + ".deps");
            std::ifstream in(file);
            if (!in) return {};
            std::vector<std::string> r;
            std::string line;
            while (std::getline(in, line)) {
                line = trim(line);
                if (line.empty() || line[0] == '#') continue;
                size_t colon = line.find(':');
                r.push_back(trim(line.substr(0, colon)));
            }
            return r;
        }
        std::string base = registrations_;
        if (!base.empty() && base.back() != '/') base += '/';
        return dependenciesFromRegistration(httpGet(base + lower(package) + "/index.json"));
    }
};

struct Graph {
    std::string root;
    std::set<std::string> nodes;
    std::set<std::pair<std::string, std::string> > edges;
};

static Graph buildGraph(const Repository &repo, const Options &o) {
    Graph g;
    g.root = o.package;
    std::queue<std::pair<std::string, int> > q;
    std::set<std::string> expanded;
    g.nodes.insert(o.package);
    q.push({o.package, 0});
    while (!q.empty()) {
        auto [p,d] = q.front();
        q.pop();
        if (!expanded.insert(lower(p)).second) continue;
        auto direct = repo.deps(p);
        if (p == o.package) {
            std::cout << "Direct dependencies:\n";
            if (direct.empty()) std::cout << "  (none)\n";
            else for (const auto &dep: direct) std::cout << "  " << dep << "\n";
        }
        if (d >= o.depth) continue;
        for (const auto &dep: direct) {
            if (dep.empty()) continue;
            g.nodes.insert(dep);
            g.edges.insert({p, dep});
            q.push({dep, d + 1});
        }
    }
    return g;
}

static std::string esc(const std::string &s) {
    std::string r;
    for (char c: s) {
        if (c == '&')r += "&amp;";
        else if (c == '<')r += "&lt;";
        else if (c == '\"')r += "&quot;";
        else r += c;
    }
    return r;
}

static void saveDot(const Graph &g, const fs::path &file) {
    std::ofstream out(file);
    if (!out) throw std::runtime_error("Cannot write " + file.string());
    out << "digraph dependencies {\n  rankdir=LR;\n  node [shape=box, style=rounded];\n";
    for (auto &n: g.nodes) out << "  \"" << n << "\";\n";
    for (auto &e: g.edges) out << "  \"" << e.first << "\" -> \"" << e.second << "\";\n";
    out << "}\n";
}

static void saveSvg(const Graph &g, const fs::path &file) {
    const int h = std::max(140, int(g.nodes.size()) * 70 + 40), w = 1000;
    std::map<std::string, int> y, x;
    int i = 0;
    for (auto &n: g.nodes) {
        y[n] = 70 + i++ * 70;
        x[n] = (n == g.root ? 50 : 770);
    }
    std::ofstream out(file);
    if (!out) throw std::runtime_error("Cannot write " + file.string());
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << w << "\" height=\"" << h << "\" viewBox=\"0 0 " << w
            << " " << h <<
            "\"><defs><marker id=\"a\" markerWidth=\"10\" markerHeight=\"10\" refX=\"9\" refY=\"3\" orient=\"auto\"><path d=\"M0,0 L0,6 L9,3z\"/></marker></defs><rect width=\"100%\" height=\"100%\" fill=\"white\"/>";
    for (auto &e: g.edges) out << "<line x1=\"" << x[e.first] + 180 << "\" y1=\"" << y[e.first] << "\" x2=\"" << x[e.
                               second] << "\" y2=\"" << y[e.second] << "\" stroke=\"#64748b\" marker-end=\"url(#a)\"/>";
    for (auto &n: g.nodes) {
        out << "<rect x=\"" << x[n] << "\" y=\"" << y[n] - 20 <<
                "\" width=\"180\" height=\"40\" rx=\"7\" fill=\"#e0f2fe\" stroke=\"#0369a1\"/><text x=\"" << x[n] + 10
                << "\" y=\"" << y[n] + 5 << "\" font-family=\"Arial\" font-size=\"14\">" << esc(n) << "</text>";
    }
    out << "</svg>\n";
}

int main(int argc, char **argv) {
    try {
        Options o = parseArgs(argc, argv);
        std::cout << "package=" << o.package << "\nrepository=" << o.repository << "\noutput=" << o.output <<
                "\nformat=" << o.format << "\ndepth=" << o.depth << "\n";
        Repository r(o.repository);
        Graph g = buildGraph(r, o);
        fs::path out = fs::u8path(o.output);
        if (out.has_parent_path()) fs::create_directories(out.parent_path());
        if (o.format == "dot")saveDot(g, out);
        else saveSvg(g, out);
        std::cout << "Saved " << g.nodes.size() << " packages and " << g.edges.size() << " dependency links to " << out.
                string() << "\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        usage(std::cerr);
        return 1;
    }
}
