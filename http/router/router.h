#pragma once
#include <utility>
#include <string>
#include <vector>
#include <regex>
#include <unordered_map>
#include "iostream"
#include "../request.h"
#include "../response.h"


namespace arch_net { namespace coin { namespace router {

typedef std::function<void(HttpRequest &req, HttpResponse &res)> HandlerFunc;

typedef std::function<HandlerFunc(HandlerFunc next)> MiddlewareType;


class utils {
public:
    static std::string trimPathPrefix(std::string pattern) {
        pattern.erase(0, pattern.find_first_not_of('/'));
        return pattern;
    }

    static std::vector<std::string> splitPattern(const std::string& pattern) {
        std::vector<std::string> result;
        auto pos = pattern.begin();
        auto end = pattern.end();
        while (pos != end) {
            auto next = std::find(pos, end, '/');
            result.emplace_back(pos, next);
            if (next == end) {
                break;
            }
            pos = next + 1;
        }
        return result;
    }
};


class Node {
public:
    std::string key;
    std::string path;
    HandlerFunc handle;
    int depth;
    std::unordered_map<std::string, Node *> children;
    bool isPattern;
    std::vector<MiddlewareType> middleware;

    Node(const std::string& _key, int _depth) : key(_key), depth(_depth), isPattern(false) {}
};

class Parameters {
public:
    std::string routeName;
};


class Tree {
public:
    Node *root;
    std::unordered_map<std::string, Node *> routes;
    Parameters parameters;

    Tree() {
        root = new Node("/", 1);
    }


    void Add(std::string pattern, HandlerFunc handle, const std::vector<MiddlewareType>& middleware = {}) {
        Node *currentNode = root;

        if (pattern != currentNode->key) {
            pattern = utils::trimPathPrefix(pattern);
            std::vector<std::string> res = utils::splitPattern(pattern);
            for (const auto& key : res) {
                auto child = currentNode->children.find(key);
                if (child == currentNode->children.end()) {
                    child = currentNode->children.emplace(key, new Node(key, currentNode->depth+1)).first;
                    if (!middleware.empty()) {
                        child->second->middleware = middleware;
                    }
                }
                currentNode = child->second;
            }
        }

        if (!middleware.empty() && currentNode->depth == 1) {
            currentNode->middleware = middleware;
        }

        currentNode->handle = std::move(handle);
        currentNode->isPattern = true;
        currentNode->path = pattern;

        if (!parameters.routeName.empty()) {
            routes[parameters.routeName] = currentNode;
        }
    }


    std::vector<Node *> Find(std::string pattern, bool isRegex) const {
        Node *node = root;
        std::vector<Node *> nodes;
        std::vector<Node *> queue;

        if (pattern == node->path) {
            nodes.push_back(node);
            return nodes;
        }

        if (!isRegex) {
            pattern = utils::trimPathPrefix(pattern);
        }

        std::vector<std::string> res = utils::splitPattern(pattern);

        for (const auto& key : res) {
            auto child = node->children.find(key);

            if (child == node->children.end() && isRegex) {
                break;
            }

            if (child == node->children.end() && !isRegex) {
                return nodes;
            }

            if (pattern == child->second->path && !isRegex) {
                nodes.push_back(child->second);
                return nodes;
            }

            node = child->second;
        }

        queue.push_back(node);

        while (!queue.empty()) {
            std::vector<Node *> queueTemp;
            for (auto n : queue) {
                if (n->isPattern) {
                    nodes.push_back(n);
                }
                for (const auto& childNode : n->children) {
                    queueTemp.push_back(childNode.second);
                }
            }

            queue = std::move(queueTemp);
        }

        return nodes;
    }


};

class Router {
public:
    Router() {}

    void Handle(std::string method, std::string path, HandlerFunc handler) {
        auto it = methods.find(method);
        if (it == methods.end()) {
            throw std::invalid_argument("invalid method: "+ method);
        }

        auto tree_it = trees_.find(method);
        Tree* tree;
        if (tree_it == trees_.end()) {
            auto new_tree = new Tree();
            tree = new_tree;
            trees_[method] = new_tree;
        } else {
            tree = tree_it->second;
        }

        if (!prefix_.empty()) {
            path = prefix_ + '/' + path;
        }

        if (!parameters_.routeName.empty()) {
            tree->parameters.routeName = parameters_.routeName;
        }

        tree->Add(path, std::move(handler), middleware_);
    }

    void handle(HttpRequest &req, HttpResponse &res, HandlerFunc handler, const std::vector<MiddlewareType>& middleware) {
        auto base = std::move(handler);
        for (auto & mid : middleware) {
            base = mid(base);
        }
        base(req, res);
    }

    void ServeHttp(HttpRequest &req, HttpResponse &res) {
        auto request_url = req.url_path();

        auto tree_it = trees_.find(req.get_method());
        if (tree_it == trees_.end()) {
            // not found handler
            return;
        }
        auto nodes = tree_it->second->Find(request_url, false);
        if (!nodes.empty()) {
            auto node = nodes[0];
            if (node->handle != nullptr) {
                if (node->path == request_url) {
                    handle(req, res, node->handle, node->middleware);
                    return;
                }

                if (node->path == request_url.substr(1)) {
                    handle(req, res, node->handle, node->middleware);
                    return;
                }
            }

        } else {
            auto parts = utils::splitPattern(request_url);
            nodes = tree_it->second->Find(parts[1], true);
            for (auto& node : nodes) {
                if (node->handle != nullptr && node->path != request_url) {
                    ParamsMapType match_params_map;
                    if (match_and_parse(request_url, node->path, match_params_map)) {
                        req.params = std::move(match_params_map);
                        handle(req, res, node->handle, node->middleware);
                        return;
                    }
                }
            }
        }

        // not found

    }


public:
    using ParamsMapType = std::unordered_map<std::string, std::string>;
    bool match_and_parse(const std::string& requestUrl, const std::string& path, ParamsMapType& matchParams) {
        std::vector<std::string> matchName;
        std::string pattern;
        std::string::size_type last = 0;
        while (last < path.size()) {
            std::string::size_type pos = path.find('/', last);
            if (pos == std::string::npos) {
                pos = path.size();
            }
            std::string str = path.substr(last, pos - last);
            if (!str.empty()) {
                size_t strLen = str.size();
                char firstChar = str[0];
                char lastChar = str[strLen - 1];
                if (firstChar == '{' && lastChar == '}') {
                    std::string matchStr = str.substr(1, strLen - 2);
                    std::string::size_type matchPos = matchStr.find(':');
                    if (matchPos != std::string::npos) {
                        matchName.push_back(matchStr.substr(0, matchPos));
                        pattern += "/(" + matchStr.substr(matchPos + 1) + ")";
                    }
                } else if (firstChar == ':') {
                    std::string matchStr = str.substr(1);
                    matchName.push_back(matchStr);
                    if (matchStr == idKey) {
                        pattern += "/(" + idPattern + ")";
                    } else {
                        pattern += "/(" + defaultPattern + ")";
                    }
                } else {
                    pattern += "/" + str;
                }
            }
            last = pos + 1;
        }

        std::regex re(pattern);
        std::smatch subMatch;
        if (std::regex_match(requestUrl, subMatch, re) && subMatch.size() - 1 == matchName.size()) {
            for (size_t i = 1; i < subMatch.size(); ++i) {
                matchParams[matchName[i-1]] = subMatch.str(i);
            }
            return true;
        }
        return false;
    }


private:
    std::string prefix_;
    std::vector<MiddlewareType> middleware_;
    std::unordered_map<std::string, Tree*> trees_;
    Parameters parameters_;
    HandlerFunc not_found_;
    const std::unordered_set<std::string> methods{"GET","HEAD","POST","PUT","PATCH","DELETE"};
    const std::string idPattern = R"([\d]+)";
    // [\d]+ 和 \d+ 的区别在于前者使用了字符集合 []，表示只要匹配 [] 中的任意一个字符就算匹配成功，而后者则只匹配数字字符，即 0 到 9。
    const std::string defaultPattern = R"([\w]+)";
    const std::string idKey = "id";
};

}}}