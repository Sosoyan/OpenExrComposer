#include "parser.h"

#include <algorithm>
#include <cassert>
#include <cctype>

using namespace std;

string Parser::Node::toString() const {
    switch(type) {
		case Node::INVALID:
            return "INVALID";
        case Node::FILEPATH:
            return path;
        case Node::CONSTANT:
            return to_string(constant);
        case Node::ADD:
            return string("(") + (left?left->toString():"null") + " + " +
                                 (right?right->toString():"null") + ")";
        case Node::SUB:
            return string("(") + (left?left->toString():"null") + " - " +
                                 (right?right->toString():"null") + ")";
        case Node::MULT:
            return string("(") + (left?left->toString():"null") + " * " +
                                 (right?right->toString():"null") + ")";
        case Node::DIV:
            return string("(") + (left?left->toString():"null") + " / " +
                                 (right?right->toString():"null") + ")";
        case Node::ASSIGN:
            return (left?left->toString():"null") + " = " + 
                   (right?right->toString():"null");
        default:
            return string("NOT IMPLEMENTED:") + to_string(type) + "this:" + to_string(size_t(this));
    }
}

void Parser::Node::evaluate(function<void(const Node* node)>& lambda) const {
	lambda(this);
}

Parser::Token::Token(char operation) : s(string(1, operation)) {
    switch(operation) {
        case '+':
            type = ADD;
            break;
        case '-':
            type = SUB;
            break;
        case '*':
            type = MULT;
            break;
        case '/':
            type = DIV;
            break;
        default:
            assert(false);
    }
};

Parser::Node::NodeType nodeTypeFromTokenType(Parser::Token::TokenType t) {
    switch(t) {
        case Parser::Token::FILEPATH:
            return Parser::Node::FILEPATH;
        case Parser::Token::CONSTANT:
            return Parser::Node::CONSTANT;
        case Parser::Token::ADD:
            return Parser::Node::ADD;
        case Parser::Token::SUB:
            return Parser::Node::SUB;
        case Parser::Token::MULT:
            return Parser::Node::MULT;
        case Parser::Token::DIV:
            return Parser::Node::DIV;
        case Parser::Token::SUBTERM:
        default:
            return Parser::Node::INVALID;
    }
}

// Removes leading and trailing whitespace from string.
string trim(const string& s) {
    string tmp = s.substr(0, s.find_last_not_of(" ") + 1);
    tmp = tmp.substr(tmp.find_first_not_of(" "));
    return tmp;
}

// If the string starts with '(' and ends with ')', removes these characters.
string unwrap(const string &s) {
    string tmp = trim(s);
    while (tmp.front() == '(' && tmp.back() == ')') {
        tmp = tmp.substr(1, tmp.size()-2);
		tmp = trim(tmp);
    }
    return tmp;
}

// Splits string at delimiter into array.
vector<string> split(string s, string delimiter) {
    size_t delimiterPos = s.find(delimiter);
    if (delimiterPos == string::npos) {
        s = trim(s);
        return vector<string>(1, s);
    }
    string left = s.substr(0, delimiterPos);
    left = trim(left);
    string right = s.substr(delimiterPos+1, string::npos);
    right = trim(right);
    vector<string> res(1, left);
    vector<string> rightElements = split(right, delimiter);
    res.insert(res.end(), rightElements.begin(), rightElements.end());
    return res;
}

// Parses string into an array off top-level Token.
vector<Parser::Token> Parser::serializeOperandsAndParentheses(string s) {
    s = unwrap(s);
    size_t currentPos = 0;
    vector<Token> result;
    while(currentPos < s.size()) {
        // skip whitespace
        currentPos = s.find_first_not_of(" ", currentPos);
        assert(currentPos != string::npos);
        size_t currentTokenEndPos = s.find_first_of("()+-*/", currentPos);
        if(currentPos == currentTokenEndPos) {
            char sign = s[currentPos];
            assert(sign != ')');
			// TODO handle malformed input
            if(s.find_first_of("+-*/", currentPos) == currentPos) {
                result.push_back(Token(sign));
                currentPos++;
            } else {
                assert(sign == '(');
                // Find matching closing parenthesis.
                int parenthesesOpened = 1;
                size_t idx = currentPos;
                while(parenthesesOpened!=0 && idx < s.size()-1) {
                    idx++;
                    char charAtIdx = s[idx];
                    if(charAtIdx == '(')
                        parenthesesOpened++;
                    else if(charAtIdx == ')')
                        parenthesesOpened--;
                }
                // TODO: Handle mismatching parentheses.
                assert(idx < s.size());
				assert(idx > currentPos);
                size_t termLength = idx+1-currentPos;
                result.push_back(Token(Token::SUBTERM, s.substr(currentPos, termLength)));
                currentPos += termLength;
            }
        } else {
            //parsing a path or constant.
            //check if string is a constant first.
            char* end = nullptr;
			const char* cs = s.c_str();
            const char* currentPosPtr = cs + currentPos;
            float converted = strtof(currentPosPtr, &end);
            if (converted != 0.0) {
				// Parsing a constan.
				// TODO: handle constants that are 0.
				assert(end > currentPosPtr);
                size_t charsParsed = end - currentPosPtr;
				string constantString = s.substr(currentPos, charsParsed);
                result.push_back(Token(Token::CONSTANT, constantString));
                currentPos += charsParsed;
            } else {
				// Parsing a path.
                std::string lower = s;
                std::transform(lower.begin(), lower.end(), lower.begin(),
                    [](unsigned char c) { return std::tolower(c); });
                size_t extensionPos = lower.find(".exr", currentPos);
                //TODO: handle invalid input here.
                assert(extensionPos != string::npos);
				assert(extensionPos > currentPos);
                size_t pathLength = extensionPos - currentPos + 4;
				string path = s.substr(currentPos, pathLength);
                result.push_back(Token(Token::FILEPATH, path));
                currentPos += pathLength;
            }
        }
    }
    return result;
}

Parser::Node* Parser::parse(string s) {
    vector<Token> serialized = serializeOperandsAndParentheses(s);
    return parse(serialized);
}

Parser::Node* Parser::parse(vector<Parser::Token> serialized) {
    if(serialized.empty()) {
        // some error happened.
        return nullptr;
    }
    assert(serialized.size() % 2 == 1);
    if(serialized.size() == 1) {
		Node* result = new Node();
        switch(serialized[0].type) {
            case Token::FILEPATH:
                result->type = Node::FILEPATH;
                result->path = serialized[0].s;
                break;
            case Token::CONSTANT:
                result->type = Node::CONSTANT;
                result->constant = strtof(serialized[0].s.c_str(), nullptr);
                break;
            case Token::SUBTERM:
                return parse(serialized[0].s);
            default:
                assert(false);
        }
		return result;
    }
    // find weakest operator, this is the root.
    int weakest = 1;
    bool weakestIsAddOrSub = serialized[weakest].isAddOrSub();
    for (int i=3; i<serialized.size(); i+=2) {
        const Token& t = serialized[i];
        assert(t.isOperator());
        if (t.isAddOrSub()) {
            weakest = i;
            weakestIsAddOrSub = true;
        } else if(!weakestIsAddOrSub) {
            weakest = i;
        }
    }
    assert(weakest < serialized.size() - 1);
	Node* result = new Node();
    result->type = nodeTypeFromTokenType(serialized[weakest].type);
    result->left = parse(vector<Token>(serialized.begin(), serialized.begin() + weakest));
    result->right = parse(vector<Token>(serialized.begin() + weakest + 1, serialized.end()));
	return result;
}

Parser::Parser(string exp)
    :_isValid(false) {
    // parse root
    string s = exp;
    if(std::count(s.begin(), s.end(), '=') != 1) {
        _errorMessage = "Expression must contain exactly one occurance of '='.";
        return;
    }

    _root.type = Node::ASSIGN;

    vector<string> rootExpressions = split(s, "=");
    assert(rootExpressions.size() == 2);
	_root.left = new Node();
	_root.left->type = Node::FILEPATH;
	_root.left->path = rootExpressions[0];

    Node* right = parse(rootExpressions[1]);
    if(!right) {
        // parsing failed. _errorMessage already contains reason.
        return;
    }

    _root.right = right;
    _isValid = true;
}