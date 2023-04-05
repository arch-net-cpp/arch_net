#include "trie.h"

namespace arch_net { namespace coin {

Node* Trie::add_node(std::string pattern) {
    trim_path_spaces(pattern);
    auto search = pattern.find("//");
    if (search != std::string::npos) {
        throw Exception("`//` is not allowed: " + pattern);
    }

    std::string tmp_pattern = trim_prefix_slash(pattern);
    std::vector<std::string> segments;
    string_split(tmp_pattern, '/', segments);

    Node* node = _insert_node(root.get(), segments);
    if (node->pattern.empty()) {
        node->pattern = pattern;
    }

    return node;
}

Node* Trie::get_colon_node(Node* parent, const std::string& segment) const{
    Node* child = parent->colon_child;

    if (child && !child->regex.empty()) {
        bool matched = is_str_match_regex(segment, child->regex);
        if (!matched) {
            child = nullptr;
        }
    }

    return child;
}

// retry to fall back to look up the colon nodes in `stack`
void Trie::fallback_lookup(FallbackStack& fallback_stack, std::vector<std::string>& segments,
                           std::unordered_map<std::string, std::string>& params, LookupResult& lr) const{
    Matched matched;
    lr = std::make_pair(false, matched);
    if (fallback_stack.empty()) {
        return;  // 返回<false, 空值>
    }

    std::pair<int, Node*> fallback = fallback_stack.back();
    fallback_stack.pop_back();

    int segment_index = fallback.first;  // index
    Node* parent = fallback.second;      // colon_node

    if (!(parent->name.empty())) {  // fallback to the colon node and fill param if matched
        matched.params[parent->name] = segments[segment_index];
    }

    mixin(params, matched.params);  // mixin params parsed before
    bool flag = true;
    for (size_t i = 0; i < segments.size(); i++) {
        if (i <= size_t(segment_index)) {  // mind: should use <= not <
            continue;
        }

        const std::string& s = segments[i];

        Node* node;
        Node* colon_node;
        bool is_same;
        std::tie(node, colon_node, is_same) = find_matched_child(parent, s);
        //if (_ignore_case && !node) {
        //    std::tie(node, colon_node, is_same) = find_matched_child(parent, string_to_lower(s));
        //}

        if (colon_node && !is_same) {
            // save colon node to fallback stack
            std::pair<int, Node*> tmp_fallback = std::make_pair(i, colon_node);
            fallback_stack.emplace_back(tmp_fallback);
        }

        if (!node) {       // both exact child and colon child is nil
            flag = false;  // should not set parent value
            break;
        }

        parent = node;
    }

    if (flag && parent->endpoint) {
        matched.node = parent;
        _get_pipeline(parent, matched.pipeline);
    }

    if (matched.node) {
        lr.first = true;
    } else {
        lr.first = false;
    }
}

// find exactly mathed node and colon node
MatchedChild Trie::find_matched_child(Node* parent, const std::string& segment) const{
    Node* child = parent->find_child(segment);
    Node* colon_node = get_colon_node(parent, segment);

    if (child) {
        if (colon_node) {
            MatchedChild m(child, colon_node, false);
            return m;
        } else {
            MatchedChild m(child, nullptr, false);
            return m;
        }
    } else {
        if (colon_node) {
            MatchedChild m(colon_node, colon_node, true);  //  后续不再压栈
            return m;
        } else {
            MatchedChild m(nullptr, nullptr, false);
            return m;
        }
    }
}

void Trie::match(const std::string& path, Matched& matched) const{
    if (path.empty()) {
        LOG(ERROR) << "`path` should not be nil or empty";
        matched.error = true;
        return;
    }

    //path = slim_path(path);
    if (path[0] != '/') {
        LOG(ERROR) << "`path` is not start with prefix /: " + path;
        matched.error = true;
        return;
    }

    _match(path, matched);

    if (!matched.node && _strict_route) {
        if (path.back() == '/') {  // retry to find path without last slash
             _match(path.substr(0, path.length() - 1), matched);
        }
    }
}

void Trie::_match(const std::string& path, Matched& matched) const{
    std::vector<std::string> segments;
    string_split(path, '/', segments);
    segments.erase(segments.begin());  // remove first empty item: ""

    bool flag = true;  // whether to continue to find matched node or not

    matched.node = nullptr;  // important! explicitly
    Node* parent = root.get();
    FallbackStack fallback_stack;

    for (size_t i = 0; i < segments.size(); i++) {
        std::string s = segments[i];
        Node* node;
        Node* colon_node;
        bool is_same;
        std::tie(node, colon_node, is_same) = find_matched_child(parent, s);
        //
        //if (_ignore_case && !node) {
        //    std::tie(node, colon_node, is_same) = find_matched_child(parent, string_to_lower(s));
        //}

        if (colon_node && !is_same) {
            fallback_stack.emplace_back(static_cast<int>(i), colon_node);
        }

        if (!node) {       //  both exact child and colon child is nil
            flag = false;  // should not set parent value
            break;
        }

        parent = node;
        if (!(parent->name.empty())) {
            matched.params[parent->name] = s;
        }
    }

    if (flag && parent->endpoint) {
        matched.node = parent;
    }

    auto params = matched.params;  // init
    bool die = false;
    if (!matched.node) {
        int depth = 0;
        bool exit = false;

        while (true) {
            depth++;
            if (depth > _max_fallback_depth) {
                die = true;
                break;
            }

            LookupResult lr ;
            fallback_lookup(fallback_stack, segments, params, lr);
            exit = lr.first;
            if (exit) {  // found
                matched = lr.second;
                break;
            }

            if (fallback_stack.empty()) {
                break;
            }
        }
    }

    if (die) {
        std::ostringstream oss;
        oss << "fallback lookup reaches the limit: " << _max_fallback_depth;
        LOG(ERROR) << oss.str();
        matched.error = true;
        return;
    }

    matched.params = std::move(params);
    if (matched.node) {
        _get_pipeline(matched.node, matched.pipeline);
    }
}

bool Trie::_check_segment(const std::string& segment) {
    bool matched = std::regex_match(segment, _uri_segment_pattern);
    return matched;
}

ValidColonChild Trie::_check_colon_child(Node* node, Node* colon_child) const{
    ValidColonChild result(false, nullptr);
    if (node == nullptr) {
        return result;
    }
    if (colon_child == nullptr) {
        return result;
    }

    int name_equal = std::strcmp(node->name.c_str(), colon_child->name.c_str());
    int regex_equal = std::strcmp(node->regex.c_str(), colon_child->regex.c_str());
    if (name_equal != 0 || regex_equal != 0) {
        result.second = colon_child;
        return result;
    }

    result.first = true;  // could be added
    return result;
}

Node* Trie::_insert_node(Node* parent, std::vector<std::string>& frags) {
    std::string& frag = frags.front();
    Node* child = _get_or_new_node(parent, frag);

    if (frags.size() >= 1) {
        frags.erase(frags.begin());
    }

    if (frags.empty()) {
        child->endpoint = true;
        return child;
    }

    return _insert_node(child, frags);
}

Node* Trie::_get_or_new_node(Node* parent, std::string& frag) {
    if (frag.empty() || std::strcmp("/", frag.c_str()) == 0) {
        frag = "";
    }

    Node* node = parent->find_child(frag);
    if (node) {
        return node;
    }

    _counter++;
    nodes.emplace_back(std::make_unique<Node>(false, _counter));
    node = nodes.back().get();

    node->parent = parent;

    if (frag.empty()) {
        NodeHolder node_pack;
        node_pack.key = frag;
        node_pack.val = node;
        parent->children.emplace_back(node_pack);
        return node;
    }
    if (frag[0] == ':') {
        std::string name = frag.substr(1);
        std::string trailing = name.substr(name.size() - 1);

        if (std::strcmp(")", trailing.c_str()) == 0) {
            auto search = name.find_first_of('(');
            if (search != std::string::npos) {
                int index = static_cast<int>(search);
                std::string regex = name.substr(index + 1, name.size() - index - 2);
                if (!regex.empty()) {
                    name = name.substr(0, index);
                    node->regex = regex;
                } else {
                    throw Exception("invalid pattern[1]: " + frag);
                }
            }
        }

        bool is_name_valid = _check_segment(name);
        if (!is_name_valid) {
            std::ostringstream oss;
            oss << "invalid pattern[2], illegal path:" << name << ", valid pattern is ["
                << _segment_pattern << "]";
            throw Exception(oss.str());
        }

        node->name = name;
        Node* colon_child = parent->colon_child;
        if (colon_child) {
            auto valid_colon_child = _check_colon_child(node, colon_child);
            if (!valid_colon_child.first) {
                std::ostringstream oss;
                oss << "invalid pattern[3]: [" << name << "] conflict with ["
                    << valid_colon_child.second->name << "]";
                throw Exception(oss.str());
            } else {
                // TODO: check here!
                // delete node;
                return colon_child;
            }
        }

        parent->colon_child = node;
    } else {
        bool is_name_valid = _check_segment(frag);
        if (!is_name_valid) {
            std::ostringstream oss;
            oss << "invalid pattern[4], " << frag << ", valid pattern is [" << _segment_pattern
                << "]";
            throw Exception(oss.str());
        }

        NodeHolder node_pack;
        node_pack.key = frag;
        node_pack.val = node;
        parent->children.emplace_back(node_pack);
    }

    return node;
}

void Trie::_get_pipeline(Node* node, std::vector<Node*>& pipeline) const{
    if (!node) return;

    std::vector<Node*> tmp;
    Node* origin_node = node;
    tmp.push_back(origin_node);
    while (node->parent) {
        tmp.push_back(node->parent);
        node = node->parent;
    }

    for (auto it = tmp.rbegin(); it != tmp.rend(); it++) {
        pipeline.emplace_back(*it);
    }
}

void Trie::set_segment_pattern(const std::string& v) {
    _segment_pattern = v;
    _uri_segment_pattern = std::regex(v);
}

void Trie::walk_trie(std::vector<std::pair<std::string, std::string>>& result) {
    std::function<void(const std::string&, Node*, std::vector<std::pair<std::string, std::string>>&)>
            recursive = [&](const std::string& prefix, Node* node,
                                  std::vector<std::pair<std::string, std::string>>& res) {
        if (node->is_root) {
            if (node->endpoint) {
                auto& handlers = node->handlers;
                for (auto& it : handlers) {
                    res.emplace_back(it.first, "/");
                }
            }
        }

        Node* colon_child = node->colon_child;
        if (colon_child) {
            std::string p = prefix + "/:" + colon_child->name;
            if (colon_child->endpoint) {
                auto& handlers = colon_child->handlers;
                for (auto& it : handlers) {
                    res.emplace_back(it.first, p);
                }
            }
            recursive(p, colon_child, res);
        }

        std::vector<NodeHolder> children = node->children;
        if (!children.empty()) {
            for (auto& nh : children) {
                if (nh.key.empty()) {
                } else {
                    if (nh.val->endpoint) {
                        std::string p = prefix + "/" + nh.key;
                        auto& handlers = nh.val->handlers;
                        for (auto& it : handlers) {
                            res.emplace_back(it.first, p);
                        }
                    }
                }

                std::string p = prefix + "/" + nh.key;
                recursive(p, nh.val, res);
            }
        }
    };

    recursive("", root.get(), result);
}

}}
