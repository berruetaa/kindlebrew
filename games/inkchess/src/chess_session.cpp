#include "chess_session.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>

namespace inkchess {

namespace {

int square_from_uci_chars(char file, char rank) {
    if (file < 'a' || file > 'h' || rank < '1' || rank > '8') return -1;
    return (rank - '1') * 8 + (file - 'a');
}

const char* terminal_label(TerminalReason reason) {
    switch (reason) {
        case TerminalReason::CHECKMATE: return "CHECKMATE";
        case TerminalReason::STALEMATE: return "STALEMATE";
        case TerminalReason::INSUFFICIENT_MATERIAL: return "DRAW - MATERIAL";
        case TerminalReason::FIVEFOLD_REPETITION: return "DRAW - FIVEFOLD";
        case TerminalReason::SEVENTY_FIVE_MOVES: return "DRAW - 75 MOVES";
        case TerminalReason::NONE: break;
    }
    return "";
}

}  // namespace

ChessSession::ChessSession() = default;

void ChessSession::clear_pending() {
    pending_ = Pending::NONE;
    promotion_moves_.clear();
    pending_move_.reset();
}

void ChessSession::reset(PlayMode mode, int elo) {
    board_ = chess::Board();
    mode_ = mode;
    elo_ = std::clamp(elo, 1320, 3190);
    outcome_ = OutcomeOverride::NONE;
    move_history_.clear();
    uci_history_.clear();
    san_history_.clear();
    selected_square_ = -1;
    target_mask_ = 0;
    last_move_mask_ = 0;
    last_san_.clear();
    clear_pending();
}

std::optional<chess::Move> ChessSession::legal_uci(std::string_view text) const {
    if (text.size() != 4 && text.size() != 5) return std::nullopt;
    const chess::Move candidate = chess::uci::uciToMove(board_, text);
    chess::Movelist legal;
    chess::movegen::legalmoves(legal, board_);
    for (const auto& move : legal) {
        if (move == candidate) return move;
    }
    return std::nullopt;
}

bool ChessSession::restore(const SaveData& save) {
    ChessSession rebuilt;
    rebuilt.reset(save.mode, save.elo);

    for (const auto& text : save.moves) {
        const auto move = rebuilt.legal_uci(text);
        if (!move) return false;
        const std::string san = chess::uci::moveToSan(rebuilt.board_, *move);
        rebuilt.move_history_.push_back(*move);
        rebuilt.uci_history_.push_back(text);
        rebuilt.san_history_.push_back(san);
        rebuilt.board_.makeMove(*move);
    }

    if (rebuilt.board_.getFen() != save.fen) return false;
    rebuilt.outcome_ = save.outcome;
    rebuilt.update_last_metadata();
    *this = std::move(rebuilt);
    return true;
}

SaveData ChessSession::save_data() const {
    SaveData data;
    data.mode = mode_;
    data.elo = elo_;
    data.outcome = outcome_;
    data.moves = uci_history_;
    data.fen = board_.getFen();
    return data;
}

bool ChessSession::human_turn() const noexcept {
    if (ended()) return false;
    if (mode_ == PlayMode::LOCAL_TWO_PLAYER) return true;
    if (mode_ == PlayMode::HUMAN_WHITE) return board_.sideToMove() == chess::Color::WHITE;
    return board_.sideToMove() == chess::Color::BLACK;
}

bool ChessSession::engine_turn() const noexcept {
    return !ended() && mode_ != PlayMode::LOCAL_TWO_PLAYER && !human_turn();
}

bool ChessSession::ended() const {
    if (outcome_ != OutcomeOverride::NONE) return true;
    return terminal_state(board_).reason != TerminalReason::NONE;
}

bool ChessSession::can_undo() const noexcept {
    if (move_history_.empty() || outcome_ != OutcomeOverride::NONE) return !move_history_.empty();
    if (mode_ == PlayMode::LOCAL_TWO_PLAYER) return true;
    if (engine_turn()) return true;
    return move_history_.size() >= 2;
}

bool ChessSession::can_claim_current() const {
    return human_turn() && selected_square_ < 0 && current_draw_claims(board_).any();
}

bool ChessSession::engine_should_claim() const {
    return engine_turn() && current_draw_claims(board_).any();
}

bool ChessSession::piece_is_current_side(int square) const {
    if (square < 0 || square >= 64) return false;
    const chess::Piece p = board_.at(chess::Square(square));
    return p != chess::Piece::NONE && p.color() == board_.sideToMove();
}

std::uint64_t ChessSession::selection_mask() const noexcept {
    std::uint64_t mask = target_mask_;
    if (selected_square_ >= 0) mask |= 1ULL << selected_square_;
    return mask;
}

void ChessSession::select(int square) {
    selected_square_ = square;
    target_mask_ = 0;
    chess::Movelist legal;
    chess::movegen::legalmoves(legal, board_);
    for (const auto& move : legal) {
        if (move.from().index() != square) continue;
        // Internal standard-chess castling points at the rook. UI selection
        // must expose the king's actual destination (g/c file).
        const std::string uci = chess::uci::moveToUci(move);
        if (uci.size() < 4) continue;
        const int target = square_from_uci_chars(uci[2], uci[3]);
        if (target >= 0) target_mask_ |= 1ULL << target;
    }
}

std::vector<chess::Move> ChessSession::moves_from_to(int from, int to) const {
    std::vector<chess::Move> result;
    chess::Movelist legal;
    chess::movegen::legalmoves(legal, board_);
    for (const auto& move : legal) {
        if (move.from().index() != from) continue;
        const std::string uci = chess::uci::moveToUci(move);
        if (uci.size() < 4) continue;
        if (square_from_uci_chars(uci[2], uci[3]) == to) result.push_back(move);
    }
    return result;
}

std::uint64_t ChessSession::compute_piece_diff(const chess::Board& before,
                                               const chess::Board& after) const {
    std::uint64_t mask = 0;
    for (int sq = 0; sq < 64; ++sq) {
        if (before.at(chess::Square(sq)) != after.at(chess::Square(sq))) mask |= 1ULL << sq;
    }
    return mask;
}

std::uint64_t ChessSession::move_display_mask(std::string_view uci) const {
    if (uci.size() < 4) return 0;
    const int from = square_from_uci_chars(uci[0], uci[1]);
    const int to = square_from_uci_chars(uci[2], uci[3]);
    std::uint64_t mask = 0;
    if (from >= 0) mask |= 1ULL << from;
    if (to >= 0) mask |= 1ULL << to;
    return mask;
}

void ChessSession::update_last_metadata() {
    if (uci_history_.empty()) {
        last_move_mask_ = 0;
        last_san_.clear();
        return;
    }
    last_move_mask_ = move_display_mask(uci_history_.back());
    last_san_ = san_history_.empty() ? uci_history_.back() : san_history_.back();
}

std::uint64_t ChessSession::commit_move(const chess::Move& move) {
    const chess::Board before = board_;
    const std::uint64_t old_ui = selection_mask();
    const std::uint64_t old_last = last_move_mask_;
    const std::string uci = chess::uci::moveToUci(move);
    const std::string san = chess::uci::moveToSan(board_, move);

    board_.makeMove(move);
    move_history_.push_back(move);
    uci_history_.push_back(uci);
    san_history_.push_back(san);
    outcome_ = OutcomeOverride::NONE;

    selected_square_ = -1;
    target_mask_ = 0;
    clear_pending();
    update_last_metadata();

    return old_ui | old_last | last_move_mask_ | compute_piece_diff(before, board_);
}

ChessSession::TapResult ChessSession::prepare_move(const chess::Move& move) {
    TapResult result;
    const DrawClaims claim = draw_claims_after_move(board_, move);
    if (claim.any()) {
        pending_move_ = move;
        pending_ = Pending::CLAIM_INTENDED;
        result.changed = true;
        result.dirty_squares = selection_mask();
        return result;
    }

    result.dirty_squares = commit_move(move);
    result.changed = true;
    result.moved = true;
    return result;
}

ChessSession::TapResult ChessSession::tap_square(int square) {
    TapResult result;
    if (!human_turn() || ended() || pending_ != Pending::NONE || square < 0 || square >= 64) return result;

    const std::uint64_t old_selection = selection_mask();

    if (selected_square_ < 0) {
        if (!piece_is_current_side(square)) return result;
        select(square);
        result.changed = true;
        result.dirty_squares = old_selection | selection_mask();
        return result;
    }

    if (piece_is_current_side(square)) {
        select(square);
        result.changed = true;
        result.dirty_squares = old_selection | selection_mask();
        return result;
    }

    const auto candidates = moves_from_to(selected_square_, square);
    if (candidates.empty()) {
        selected_square_ = -1;
        target_mask_ = 0;
        result.changed = true;
        result.dirty_squares = old_selection;
        return result;
    }

    if (candidates.size() > 1) {
        bool all_promotion = true;
        for (const auto& move : candidates) {
            if (move.typeOf() != chess::Move::PROMOTION) all_promotion = false;
        }
        if (all_promotion) {
            promotion_moves_ = candidates;
            pending_ = Pending::PROMOTION;
            result.changed = true;
            result.dirty_squares = old_selection;
            return result;
        }
    }

    return prepare_move(candidates.front());
}

ChessSession::TapResult ChessSession::choose_promotion(chess::PieceType type) {
    TapResult result;
    if (pending_ != Pending::PROMOTION) return result;
    for (const auto& move : promotion_moves_) {
        if (move.promotionType() == type) return prepare_move(move);
    }
    return result;
}

ChessSession::TapResult ChessSession::play_pending_claim_move() {
    TapResult result;
    if (pending_ != Pending::CLAIM_INTENDED || !pending_move_) return result;
    const chess::Move move = *pending_move_;
    // The user explicitly declined a valid pre-move claim. Commit this exact
    // move without re-running the same claim gate.
    result.dirty_squares = commit_move(move);
    result.changed = true;
    result.moved = true;
    return result;
}

bool ChessSession::claim_pending_draw() {
    if (pending_ != Pending::CLAIM_INTENDED || !pending_move_) return false;
    const DrawClaims claims = draw_claims_after_move(board_, *pending_move_);
    if (!claims.any()) return false;
    outcome_ = OutcomeOverride::DRAW_CLAIM;
    selected_square_ = -1;
    target_mask_ = 0;
    clear_pending();
    return true;
}

bool ChessSession::claim_current_draw() {
    if (!can_claim_current()) return false;
    outcome_ = OutcomeOverride::DRAW_CLAIM;
    selected_square_ = -1;
    target_mask_ = 0;
    clear_pending();
    return true;
}

bool ChessSession::apply_engine_move(std::string_view uci, std::uint64_t* dirty_squares) {
    if (!engine_turn() || ended()) return false;
    const auto move = legal_uci(uci);
    if (!move) return false;
    const std::uint64_t dirty = commit_move(*move);
    if (dirty_squares) *dirty_squares = dirty;
    return true;
}

std::uint64_t ChessSession::undo() {
    if (!can_undo() || move_history_.empty()) return 0;

    const chess::Board before = board_;
    const std::uint64_t old_ui = selection_mask();
    const std::uint64_t old_last = last_move_mask_;
    int plies = 1;

    if (mode_ != PlayMode::LOCAL_TWO_PLAYER && human_turn() && move_history_.size() >= 2) {
        plies = 2;
    }

    for (int i = 0; i < plies && !move_history_.empty(); ++i) {
        const chess::Move move = move_history_.back();
        board_.unmakeMove(move);
        move_history_.pop_back();
        uci_history_.pop_back();
        if (!san_history_.empty()) san_history_.pop_back();
    }

    outcome_ = OutcomeOverride::NONE;
    selected_square_ = -1;
    target_mask_ = 0;
    clear_pending();
    update_last_metadata();
    return old_ui | old_last | last_move_mask_ | compute_piece_diff(before, board_);
}

void ChessSession::resign() {
    if (ended()) return;
    outcome_ = board_.sideToMove() == chess::Color::WHITE
                   ? OutcomeOverride::BLACK_WIN_RESIGN
                   : OutcomeOverride::WHITE_WIN_RESIGN;
    selected_square_ = -1;
    target_mask_ = 0;
    clear_pending();
}

TerminalState ChessSession::automatic_terminal() const {
    return terminal_state(board_);
}

std::string ChessSession::status_text() const {
    if (outcome_ == OutcomeOverride::DRAW_CLAIM) return "DRAW CLAIMED";
    if (outcome_ == OutcomeOverride::WHITE_WIN_RESIGN) return "WHITE WINS";
    if (outcome_ == OutcomeOverride::BLACK_WIN_RESIGN) return "BLACK WINS";

    const TerminalState terminal = automatic_terminal();
    if (terminal.reason == TerminalReason::CHECKMATE) {
        return board_.sideToMove() == chess::Color::WHITE ? "BLACK WINS - MATE" : "WHITE WINS - MATE";
    }
    if (terminal.reason != TerminalReason::NONE) return terminal_label(terminal.reason);

    return board_.sideToMove() == chess::Color::WHITE ? "WHITE TO MOVE" : "BLACK TO MOVE";
}

std::string ChessSession::secondary_text(bool engine_available, bool engine_thinking) const {
    if (ended()) return last_san_;
    if (engine_thinking) return "STOCKFISH THINKING";
    if (!engine_available && mode_ != PlayMode::LOCAL_TWO_PLAYER) return "ENGINE UNAVAILABLE";
    if (!last_san_.empty()) return "LAST " + last_san_;
    if (mode_ == PlayMode::LOCAL_TWO_PLAYER) return "LOCAL TWO PLAYER";
    return "STOCKFISH " + std::to_string(elo_);
}

}  // namespace inkchess
