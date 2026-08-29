/****************************************************************************
 * 国标麻将算番 Demo
 * 基于 summerinsects/mahjong-algorithm
 *
 * 用法:
 *   交互模式: ./demo                然后按提示输入
 *   单局模式:   ./demo "<手牌字符串>" [选项]
 *
 * 选项 (单局模式, 形如 key=value):
 *   win=self|discard|kong|wall|initial   和牌方式 (默认 discard)
 *   prevalent=east|south|west|north       圈风 (默认 east)
 *   seat=east|south|west|north            门风 (默认 east)
 *   flower=<n>                            花牌数 (默认 0)
 *
 * 手牌字符串示例:
 *   1112345678999s9s              纯立牌
 *   [EEEE][CCCC][FFFF][PPPP]NN    4副露+立牌
 *   [345m3]258m1488s369p7s        带吃/碰副露
 *   数牌: 万=m 条=s 饼=p   字牌: 东南西北=ESWN 中发白=CFP
 *   吃/碰/杠用 [xxx] 表示, 详情见 README.md
 ****************************************************************************/

#include "tile.h"
#include "shanten.h"
#include "stringify.h"
#include "fan_calculator.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace mahjong;

// ---------- 工具函数 ----------

// 选项解析: 在 argv 中查找 key=value, 返回 value
static std::string get_opt(int argc, char **argv, const char *key, const std::string &def) {
    size_t klen = std::strlen(key);
    for (int i = 2; i < argc; ++i) {
        if (std::strncmp(argv[i], key, klen) == 0 && argv[i][klen] == '=') {
            return argv[i] + klen + 1;
        }
    }
    return def;
}

static wind_t parse_wind(const std::string &s) {
    if (s == "east")  return wind_t::EAST;
    if (s == "south") return wind_t::SOUTH;
    if (s == "west")  return wind_t::WEST;
    if (s == "north") return wind_t::NORTH;
    return wind_t::EAST;
}

static const char *wind_name(wind_t w) {
    switch (w) {
        case wind_t::EAST:  return "东";
        case wind_t::SOUTH: return "南";
        case wind_t::WEST:  return "西";
        case wind_t::NORTH: return "北";
    }
    return "?";
}

static win_flag_t parse_win_flag(const std::string &s) {
    // 支持组合, 如 self,wall
    win_flag_t flag = WIN_FLAG_DISCARD;
    if (s.find("self") != std::string::npos)    flag |= WIN_FLAG_SELF_DRAWN;
    if (s.find("kong") != std::string::npos)    flag |= WIN_FLAG_KONG_INVOLVED;
    if (s.find("wall") != std::string::npos)    flag |= WIN_FLAG_WALL_LAST;
    if (s.find("initial") != std::string::npos) flag |= WIN_FLAG_INITIAL;
    if (s.find("last") != std::string::npos)    flag |= WIN_FLAG_LAST_TILE;
    return flag;
}

static const char *parse_err_msg(int err) {
    switch (err) {
        case PARSE_ERROR_ILLEGAL_CHARACTER:                return "非法字符";
        case PARSE_ERROR_SUFFIX:                           return "后缀错误";
        case PARSE_ERROR_WRONG_TILES_COUNT_FOR_FIXED_PACK: return "副露牌数错误";
        case PARSE_ERROR_CANNOT_MAKE_FIXED_PACK:           return "无法解析副露";
        case PARSE_ERROR_TOO_MANY_FIXED_PACKS:             return "副露过多(最多4组)";
        case PARSE_ERROR_TOO_MANY_TILES:                   return "牌数过多";
        case PARSE_ERROR_TILE_COUNT_GREATER_THAN_4:        return "某张牌超过4枚";
        default:                                           return "未知解析错误";
    }
}

static const char *calc_err_msg(int err) {
    switch (err) {
        case ERROR_WRONG_TILES_COUNT:      return "张数错误(立牌+3*副露应=13)";
        case ERROR_TILE_MORE_THAN_4:       return "某张牌出现超过4枚";
        case ERROR_NOT_WIN:                return "没有和牌";
        default:                           return "未知错误";
    }
}

// 国标计分: 按番数查表得分
struct score_row { int fan; int base; };  // 番数下限 -> 基本分
static const score_row score_table[] = {
    {88, 400}, {64, 320}, {48, 256}, {32, 192},
    {24, 144}, {16, 96},  {12, 72},  {8,  48},
    {6,  30},  {5,  24},  {4,  18},  {3,  12},
    {2,  9},   {1,  8},
};
static const int score_table_len = sizeof(score_table) / sizeof(score_table[0]);

// 返回 (基本分, 区间下限描述)
static int fan_to_base_score(int fan) {
    for (int i = 0; i < score_table_len; ++i) {
        if (fan >= score_table[i].fan) return score_table[i].base;
    }
    return 8;  // < 1 番
}

// JSON 字符串转义
static std::string json_escape(const char *s) {
    std::string out;
    for (const char *p = s; *p; ++p) {
        unsigned char c = static_cast<unsigned char>(*p);
        if (c == '\\' || c == '"') { out.push_back('\\'); out.push_back(c); }
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 0x20) { char b[8]; std::snprintf(b, sizeof(b), "\\u%04x", c); out += b; }
        else out.push_back(c);
    }
    return out;
}

// 计算结果(供两种输出模式共用)
struct calc_result {
    bool ok;
    std::string normalized;          // 规范化手牌
    std::string error;               // 错误信息
    int points;                      // 总番数
    int base_score;                  // 基本分
    struct fan_item { std::string name; int value; int count; };
    std::vector<fan_item> fans;
};

// 核心算番
static calc_result do_calculate_core(const char *str, win_flag_t win_flag,
                                     wind_t prevalent, wind_t seat, int flower) {
    calc_result r;
    calculate_param_t param;
    int ret = parse_hand_tiles(str, std::strlen(str), &param.hand_tiles, &param.win_tile);
    if (ret != PARSE_NO_ERROR) {
        r.ok = false;
        r.points = 0;
        r.base_score = 0;
        r.error = parse_err_msg(ret);
        return r;
    }

    char buf[64];
    hand_tiles_to_string(&param.hand_tiles, buf, sizeof(buf));
    r.normalized = buf;

    param.flower_count = static_cast<uint8_t>(flower);
    param.win_flag = win_flag;
    param.prevalent_wind = prevalent;
    param.seat_wind = seat;

    fan_table_t fan_table = {0};
    int points = calculate_fan(&param, &fan_table);

    if (points < 0) {
        r.ok = false;
        r.points = 0;
        r.base_score = 0;
        r.error = calc_err_msg(points);
        return r;
    }

    r.ok = true;
    r.points = points;
    r.base_score = fan_to_base_score(points);
    for (int i = 1; i < FAN_TABLE_SIZE; ++i) {
        if (fan_table[i] == 0) continue;
        r.fans.push_back({fan_name<>::text[i], fan_value<>::table[i], fan_table[i]});
    }
    return r;
}

// 人类可读输出(原终端表格)
static void print_human(const calc_result &r) {
    if (!r.ok) {
        std::printf("❌ 失败: %s\n", r.error.c_str());
        return;
    }
    std::printf("规范化手牌: %s\n", r.normalized.c_str());
    std::printf("┌────────────────────────────────────────┐\n");
    std::printf("│ 总番数: %-3d 番", r.points);
    std::printf("                          │\n");
    std::printf("├──────────────┬──────────┬──────────────┤\n");
    std::printf("│ 番种         │ 番值     │ 出现次数     │\n");
    std::printf("├──────────────┼──────────┼──────────────┤\n");
    for (const auto &f : r.fans) {
        std::printf("│ %-10s   │ %2d*%-3d    │ x%-4d        │\n",
                    f.name.c_str(), f.value, f.count, f.count);
    }
    int base = r.base_score;
    std::printf("├──────────────┴──────────┴──────────────┤\n");
    std::printf("│ 计分: 番数 %d -> 基本分 %d            │\n", r.points, base);
    std::printf("│   庄家自摸: 3家各付 %d              │\n", base);
    std::printf("│   闲家自摸: 庄家付 %d, 另2家各付 %d │\n", base, base);
    std::printf("│   点和: 放铳者付 %d                 │\n", 2 * base);
    std::printf("└────────────────────────────────────────┘\n");
}

// JSON 输出(供 Web 后端解析)
static void print_json(const calc_result &r) {
    std::printf("{");
    std::printf("\"ok\":%s", r.ok ? "true" : "false");
    std::printf(",\"normalized\":\"%s\"", json_escape(r.normalized.c_str()).c_str());
    std::printf(",\"points\":%d", r.points);
    std::printf(",\"base_score\":%d", r.base_score);
    std::printf(",\"error\":\"%s\"", json_escape(r.error.c_str()).c_str());
    std::printf(",\"fans\":[");
    for (size_t i = 0; i < r.fans.size(); ++i) {
        if (i) std::printf(",");
        std::printf("{\"name\":\"%s\",\"value\":%d,\"count\":%d}",
                    json_escape(r.fans[i].name.c_str()).c_str(),
                    r.fans[i].value, r.fans[i].count);
    }
    std::printf("]}");
    std::printf("\n");
}

// ---------- 帮助 ----------
static void print_help() {
    std::puts(
        "================ 国标麻将算番 Demo ================\n"
        "手牌字符串格式:\n"
        "  数牌: 万=m 条=s 饼=p  (同花色可连写, 如 123m)\n"
        "  字牌: 东南西北=ESWN   中发白=CFP\n"
        "  副露: 吃/碰/杠用 [xxx] 包裹, 可加数字标来源\n"
        "        吃: [567m2]=57万吃6万(第2张)\n"
        "        碰: [999s3]=碰下家的9条\n"
        "        杠: [SSSS]=暗杠南  [8888p1]=明杠上家8饼\n"
        "\n示例 (直接输入):"
    );
    const char *samples[] = {
        "1112345678999s9s",          // 九莲宝灯
        "[EEEE][CCCC][FFFF][PPPP]NN",// 字一色相关
        "[345m3]258m1488s369p7s",    // 组合龙
        "4445677m777s777p7m",        // 三暗刻等
    };
    for (const char *s : samples) {
        std::printf("  %s\n", s);
    }
    std::puts(
        "\n命令:\n"
        "  <手牌>              算番 (默认点和/东圈东门)\n"
        "  win=self            改为自摸 (下次输入生效)\n"
        "  prevalent=south      改圈风\n"
        "  seat=west            改门风\n"
        "  flower=2             设花牌数\n"
        "  help                 显示帮助\n"
        "  quit                 退出\n"
        "==================================================="
    );
}

// ---------- main ----------
int main(int argc, char **argv) {
    // 检测 --json 开关(可出现在任意位置,优先于其它处理)
    bool json_mode = false;
    int hand_arg = 1;
    if (argc >= 2 && std::strcmp(argv[1], "--json") == 0) {
        json_mode = true;
        hand_arg = 2;
    }

    // 单局模式: ./demo [--json] "<手牌>" [options...]
    if (argc > hand_arg && std::strcmp(argv[hand_arg], "--help") != 0 && argv[hand_arg][0] != '\0') {
        const char *hs = argv[hand_arg];
        win_flag_t flag = parse_win_flag(get_opt(argc, argv, "win", "discard"));
        wind_t prev = parse_wind(get_opt(argc, argv, "prevalent", "east"));
        wind_t seat = parse_wind(get_opt(argc, argv, "seat", "east"));
        int flower = std::atoi(get_opt(argc, argv, "flower", "0").c_str());

        calc_result r = do_calculate_core(hs, flag, prev, seat, flower);
        if (json_mode) {
            print_json(r);
        } else {
            std::printf("手牌: %s\n", hs);
            print_human(r);
        }
        return r.ok ? 0 : 1;
    }

    // 交互模式
    print_help();

    // 会话默认设置
    win_flag_t cur_flag = WIN_FLAG_DISCARD;
    wind_t cur_prev = wind_t::EAST;
    wind_t cur_seat = wind_t::EAST;
    int cur_flower = 0;

    std::string line;
    while (true) {
        std::printf("\n[当前: win=%s 圈风=%s 门风=%s 花=%d]\n输入手牌或命令> ",
                    (cur_flag & WIN_FLAG_SELF_DRAWN) ? "self" : "discard",
                    wind_name(cur_prev), wind_name(cur_seat), cur_flower);
        if (!std::getline(std::cin, line)) break;
        // 去除首尾空白
        size_t a = line.find_first_not_of(" \t");
        if (a == std::string::npos) continue;
        size_t b = line.find_last_not_of(" \t\r\n");
        line = line.substr(a, b - a + 1);

        if (line == "quit" || line == "exit") break;
        if (line == "help") { print_help(); continue; }

        // 内联命令 win=/prevalent=/seat=/flower=
        if (line.rfind("win=", 0) == 0) {
            cur_flag = parse_win_flag(line.substr(4) == "" ? "discard" : line.substr(4));
            std::printf("✓ 和牌方式已更新\n");
            continue;
        }
        if (line.rfind("prevalent=", 0) == 0) {
            cur_prev = parse_wind(line.substr(10));
            std::printf("✓ 圈风已更新: %s\n", wind_name(cur_prev));
            continue;
        }
        if (line.rfind("seat=", 0) == 0) {
            cur_seat = parse_wind(line.substr(5));
            std::printf("✓ 门风已更新: %s\n", wind_name(cur_seat));
            continue;
        }
        if (line.rfind("flower=", 0) == 0) {
            cur_flower = std::atoi(line.substr(7).c_str());
            std::printf("✓ 花牌数已更新: %d\n", cur_flower);
            continue;
        }

        // 否则当作手牌字符串
        calc_result r = do_calculate_core(line.c_str(), cur_flag, cur_prev, cur_seat, cur_flower);
        print_human(r);
    }

    std::puts("再见!");
    return 0;
}
