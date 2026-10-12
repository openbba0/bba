package com.android.bb.utils;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class TrieTree {

    private final TrieNode root = new TrieNode();

    private static final class TrieNode {
        final Map<Character, TrieNode> children = new HashMap<>();
        String word;
        boolean isEnd;
    }

    public void add(String word) {
        if (word == null || word.isEmpty()) {
            return;
        }
        TrieNode current = root;
        for (int index = 0; index < word.length(); index++) {
            char content = word.charAt(index);
            TrieNode next = current.children.get(content);
            if (next == null) {
                next = new TrieNode();
                current.children.put(content, next);
            }
            current = next;
        }
        current.isEnd = true;
        current.word = word;
    }

    public void addAll(List<String> words) {
        for (String word : words) {
            add(word);
        }
    }

    public String search(String word) {
        if (word == null || word.isEmpty()) {
            return null;
        }
        TrieNode current = root;
        String found = null;
        for (int index = 0; index < word.length(); index++) {
            current = current.children.get(word.charAt(index));
            if (current == null) {
                break;
            }
            if (current.isEnd && (index == word.length() - 1 || word.charAt(index + 1) == '/')) {
                found = current.word;
            }
        }
        return found;
    }
}
