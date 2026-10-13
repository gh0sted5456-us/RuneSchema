#pragma once
#include <cstddef>
#include <string>

namespace DragonWilds::DialogueCompletionPresentation {
// A completion action must own its response. Its authored Next node may also
// be reached by unrelated choices and must never be used as an error surface.
enum class Outcome { Pending, Granted, AlreadyComplete, NotReady, Uncertain, Failed };
inline constexpr const char* Message(Outcome outcome) {
    switch(outcome) {
        case Outcome::Granted:
        case Outcome::AlreadyComplete:
            return "The exchange is complete. We can continue.";
        case Outcome::NotReady:
            return "Please make room in your inventory and speak to me again. Your reward has not been claimed.";
        case Outcome::Uncertain:
            return "Our previous exchange could not be confirmed. The reward is paused to prevent giving it twice.";
        case Outcome::Pending:
        case Outcome::Failed:
            return "I could not confirm that exchange. No reward has been confirmed. Please speak to me again.";
    }
    return "I could not confirm that exchange. No reward has been confirmed. Please speak to me again.";
}
inline constexpr const char* Continue = "Continue.";
struct Route {
    std::string Response, ContinueBranch, ContinueOption, AuthoredNext;
};
inline Route MakeRoute(std::size_t index,const std::string& authoredNext) {
    const auto response="completion-response:"+std::to_string(index);
    return {response,response+":continue",response+":continue:label","text:"+authoredNext};
}
}
