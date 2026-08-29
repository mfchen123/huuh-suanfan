// 算番库入口 - 合并 countFan API 和番名常量表
const api = require("gb-mahjong-js/lib/api/index.js");
const constants = require("gb-mahjong-js/lib/core/constants.js");

module.exports = {
    ...api,
    FAN_NAME: constants.FAN_NAME,
    FAN_SCORE: constants.FAN_SCORE,
};
