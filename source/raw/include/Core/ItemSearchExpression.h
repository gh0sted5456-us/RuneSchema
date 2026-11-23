#pragma once
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
// Boolean item search, shared by Helpy and the RuneSchema core (keep both copies identical).
//   sword shield     both terms: implicit AND, the original behaviour
//   sword OR axe     either term; "|" works too
//   -rusty           exclude a term; "NOT rusty" and "!rusty" work too
//   (sword OR axe) -rusty   parentheses group
//   "iron sword"     exact phrase
// Operators are upper case, so ordinary lower-case words stay search terms. NOT binds
// tightest, then AND, then OR. Malformed input never throws, because search runs while
// the player types: a missing ")" closes at the end, a stray ")" or dangling operator is ignored.
namespace PS::ItemSearchExpression {
struct Node {
    enum class Kind:unsigned char{Term,All,Any,Not};
    Kind Type=Kind::Term;std::string Text;std::vector<Node> Children;
};
class Expression {
public:
    // fold normalises each term exactly as the caller normalises the text it matches against.
    // A term that folds to nothing is dropped, along with any operator that only applied to it.
    template<class Fold> static Expression Parse(std::string_view text,Fold&& fold){
        Expression result;Parser parser{Lex(text)};
        std::vector<Node> parts;
        while(parser.At<parser.Tokens.size()){
            if(auto part=parser.ParseOr(fold,0))parts.push_back(std::move(*part));
            if(parser.At<parser.Tokens.size())++parser.At; // Stray ")" at the top level.
        }
        result.Root=Combine(Node::Kind::All,std::move(parts));
        if(result.Root)result.Collect(*result.Root,true);
        return result;
    }
    bool Empty()const{return !Root.has_value();}
    // True when the query is only positive terms joined by AND: the original word search.
    bool Plain()const{return Simple;}
    // Terms that every match contains. Index and prefix filters may narrow by these; for a
    // plain query this is every term, in order.
    const std::vector<std::string>& Required()const{return Needed;}
    // Every term, positive or negated, for callers that only need to know what was typed.
    const std::vector<std::string>& Terms()const{return All;}
    bool Matches(std::string_view text)const{return !Root||Evaluate(*Root,text);}
private:
    enum class Token:unsigned char{Open,Close,Or,And,Not,Term};
    struct Lexeme{Token Type;std::string Text;bool Phrase=false;};
    static constexpr int MaxDepth=16;
    std::optional<Node> Root;std::vector<std::string> Needed,All;bool Simple=true;
    static bool Space(char c){return std::isspace(static_cast<unsigned char>(c))!=0;}
    static bool Delimiter(char c){return Space(c)||c=='('||c==')'||c=='"'||c=='|';}
    static std::vector<Lexeme> Lex(std::string_view text){
        std::vector<Lexeme> out;size_t i=0,open=0;
        while(i<text.size()){
            const char c=text[i];
            if(Space(c)){++i;continue;}
            if(c=='('){out.push_back({Token::Open,{}});++open;++i;continue;}
            // An unmatched ")" is dropped, so "sword) OR axe" reads as "sword OR axe".
            if(c==')'){if(open){out.push_back({Token::Close,{}});--open;}++i;continue;}
            if(c=='|'){out.push_back({Token::Or,{}});while(i<text.size()&&text[i]=='|')++i;continue;}
            if(c=='"'){
                const auto end=text.find('"',i+1);const auto inner=text.substr(i+1,end==text.npos?text.npos:end-i-1);
                out.push_back({Token::Term,std::string(inner),true});i=end==text.npos?text.size():end+1;continue;
            }
            // "-word" and "!word" negate; a lone "-" or a hyphen inside a word ("two-handed") is text.
            if((c=='-'||c=='!')&&i+1<text.size()&&!Space(text[i+1])&&text[i+1]!=c){out.push_back({Token::Not,{}});++i;continue;}
            size_t end=i;while(end<text.size()&&!Delimiter(text[end]))++end;
            const auto word=text.substr(i,end-i);i=end;
            if(word=="OR")out.push_back({Token::Or,{}});
            else if(word=="AND"||word=="&"||word=="&&")out.push_back({Token::And,{}});
            else if(word=="NOT")out.push_back({Token::Not,{}});
            else out.push_back({Token::Term,std::string(word)});
        }
        return out;
    }
    static std::optional<Node> Combine(Node::Kind kind,std::vector<Node> parts){
        if(parts.empty())return std::nullopt;
        if(parts.size()==1)return std::move(parts.front());
        Node node;node.Type=kind;
        for(auto& part:parts){
            if(part.Type==kind)for(auto& child:part.Children)node.Children.push_back(std::move(child));
            else node.Children.push_back(std::move(part));
        }
        return node;
    }
    struct Parser {
        std::vector<Lexeme> Tokens;size_t At=0;
        bool Is(Token type)const{return At<Tokens.size()&&Tokens[At].Type==type;}
        template<class Fold> std::optional<Node> ParseOr(Fold& fold,int depth){
            std::vector<Node> parts;
            if(auto first=ParseAnd(fold,depth))parts.push_back(std::move(*first));
            while(Is(Token::Or)){
                ++At;if(auto next=ParseAnd(fold,depth))parts.push_back(std::move(*next));
            }
            return Combine(Node::Kind::Any,std::move(parts));
        }
        template<class Fold> std::optional<Node> ParseAnd(Fold& fold,int depth){
            std::vector<Node> parts;
            while(At<Tokens.size()){
                if(Is(Token::And)){++At;continue;}
                if(Is(Token::Or)||Is(Token::Close))break;
                if(auto part=ParseUnary(fold,depth))parts.push_back(std::move(*part));
            }
            return Combine(Node::Kind::All,std::move(parts));
        }
        template<class Fold> std::optional<Node> ParseUnary(Fold& fold,int depth){
            bool negate=false;while(Is(Token::Not)){negate=!negate;++At;}
            auto inner=ParsePrimary(fold,depth);
            if(!inner||!negate)return inner;
            if(inner->Type==Node::Kind::Not)return std::move(inner->Children.front());
            Node node;node.Type=Node::Kind::Not;node.Children.push_back(std::move(*inner));return node;
        }
        template<class Fold> std::optional<Node> ParsePrimary(Fold& fold,int depth){
            if(At>=Tokens.size())return std::nullopt;
            auto& token=Tokens[At];
            if(token.Type==Token::Open){
                ++At;
                // Past the depth bound a "(" is ignored rather than recursed into.
                if(depth>=MaxDepth)return std::nullopt;
                auto inner=ParseOr(fold,depth+1);if(Is(Token::Close))++At;return inner;
            }
            if(token.Type!=Token::Term){++At;return std::nullopt;}
            ++At;std::string folded=fold(std::string(token.Text));
            if(folded.empty())return std::nullopt;
            Node node;node.Text=std::move(folded);return node;
        }
    };
    void Collect(const Node& node,bool required){
        if(node.Type==Node::Kind::Term){All.push_back(node.Text);if(required)Needed.push_back(node.Text);return;}
        if(node.Type!=Node::Kind::All)Simple=false;
        for(const auto& child:node.Children){
            if(child.Type==Node::Kind::Not||child.Type==Node::Kind::Any)Simple=false;
            Collect(child,required&&node.Type==Node::Kind::All);
        }
    }
    static bool Evaluate(const Node& node,std::string_view text){
        switch(node.Type){
        case Node::Kind::Term:return text.find(node.Text)!=text.npos;
        case Node::Kind::Not:return !Evaluate(node.Children.front(),text);
        case Node::Kind::All:for(const auto& child:node.Children)if(!Evaluate(child,text))return false;return true;
        case Node::Kind::Any:for(const auto& child:node.Children)if(Evaluate(child,text))return true;return false;
        }
        return false;
    }
};
}
