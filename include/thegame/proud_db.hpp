#pragma once

#include <cstdint>
#include <string>

namespace thegame {

constexpr const char *kProudDbJson = "proud_db.json";

std::string proud_db_path();

void init_proud_db();

} // namespace thegame

void generate_frame_comment(uint8_t opcode, uint16_t rmi_id, bool has_rmi,
                            bool &known, std::string &comment);
