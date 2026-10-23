// protocol.cpp - Game rules, CRC-8, frame encoding and parsing.
#include "protocol.h"

namespace rps {

Outcome judge(Move mine, Move theirs) {
    if (mine == theirs) return Outcome::Tie;
    // Rock(0) < Paper(1) < Scissors(2) < Rock(0): a move beats the one "below" it.
    uint8_t diff = (static_cast<uint8_t>(mine) + 3 - static_cast<uint8_t>(theirs)) % 3;
    return diff == 1 ? Outcome::Win : Outcome::Lose;
}

const char* moveName(Move m) {
    switch (m) {
        case Move::Rock:     return "ROCK";
        case Move::Paper:    return "PAPER";
        case Move::Scissors: return "SCISSORS";
        default:             return "?";
    }
}

Move nextMove(Move m) {
    switch (m) {
        case Move::Rock:  return Move::Paper;
        case Move::Paper: return Move::Scissors;
        default:          return Move::Rock;
    }
}

// CRC-8, polynomial 0x07 (CRC-8/SMBUS). Bitwise version: tiny code size on AVR.
uint8_t crc8(const uint8_t* data, uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07) : static_cast<uint8_t>(crc << 1);
    }
    return crc;
}

void encode(const Frame& f, uint8_t out[kFrameLen]) {
    out[0] = kSync;
    out[1] = static_cast<uint8_t>(f.type);
    out[2] = f.round;
    out[3] = f.payload;
    out[4] = crc8(&out[1], 3);
}

static bool isValid(uint8_t type, uint8_t payload) {
    if (type == static_cast<uint8_t>(MsgType::Ready)) return true;
    if (type == static_cast<uint8_t>(MsgType::Reveal)) return payload <= 2;
    return false;
}

bool FrameParser::feed(uint8_t byte, Frame& out) {
    if (!inFrame_) {
        if (byte == kSync) { inFrame_ = true; idx_ = 0; }
        return false;  // ignore noise until a sync byte arrives
    }
    buf_[idx_++] = byte;
    if (idx_ < kFrameLen - 1) return false;

    inFrame_ = false;
    if (crc8(buf_, 3) != buf_[3] || !isValid(buf_[0], buf_[2])) {
        ++errors_;
        // A lost byte means we may have swallowed the next frame's sync byte.
        // Resynchronize: restart from the first sync byte inside the buffer.
        for (uint8_t i = 0; i < kFrameLen - 1; ++i) {
            if (buf_[i] != kSync) continue;
            idx_ = 0;
            for (uint8_t j = i + 1; j < kFrameLen - 1; ++j) buf_[idx_++] = buf_[j];
            inFrame_ = true;
            break;
        }
        return false;
    }
    out.type = static_cast<MsgType>(buf_[0]);
    out.round = buf_[1];
    out.payload = buf_[2];
    return true;
}

}  // namespace rps
