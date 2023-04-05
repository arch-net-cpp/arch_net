#pragma once
#include "define.h"
#include "node.h"
#include "../common.h"

namespace arch_net { namespace coin {

typedef std::vector<std::pair<int, Node*>> FallbackStack;
typedef std::tuple<Node*, Node*, bool> MatchedChild;
typedef std::pair<bool, Node*> ValidColonChild;
typedef std::pair<bool, Matched> LookupResult;

class Trie {
public:
    // bettor to hide these properties.
    bool _ignore_case;
    bool _strict_route;
    std::string _segment_pattern;
    int _max_uri_segments;
    int _max_fallback_depth;
    // the root Node of trie
    std::unique_ptr<Node> root;
    int _counter;
    std::vector<std::unique_ptr<Node>> nodes;

private:
    std::regex _uri_segment_pattern;

public:
    Trie(bool ignore_case = true, bool strict_route = true,
         const std::string& segment_pattern = "[A-Za-z0-9._~\\-]+", int max_uri_segments = 100,
         int max_fallback_depth = 100)
            : _ignore_case(ignore_case),
              _strict_route(strict_route),
              _segment_pattern(segment_pattern),
              _max_uri_segments(max_uri_segments),
              _max_fallback_depth(max_fallback_depth) {
        _uri_segment_pattern = std::regex(segment_pattern);
        root = std::make_unique<Node>(true);
        _counter = 0;
    }

    ~Trie() {}
    Node* add_node(std::string pattern);
    void match(const std::string& path, Matched& matched) const;

private:
    Node* get_colon_node(Node* parent, const std::string& segment) const ;
    void fallback_lookup(FallbackStack& fallback_stack, std::vector<std::string>& segments,
                                 std::unordered_map<std::string, std::string>& params, LookupResult& lr) const;
    MatchedChild find_matched_child(Node* parent, const std::string& segment) const;

    bool _check_segment(const std::string& segment);
    ValidColonChild _check_colon_child(Node* node, Node* colon_child) const;
    Node* _get_or_new_node(Node* parent, std::string& frag);
    Node* _insert_node(Node* parent, std::vector<std::string>& frags);
    void _get_pipeline(Node* node, std::vector<Node*>& pipeline) const ;
    void _match(const std::string& path, Matched& matched) const;
    void set_segment_pattern(const std::string& v);
public:
    // walk tree
    void walk_trie(std::vector<std::pair<std::string, std::string>>& result);
};
}}