#pragma once

#include <QString>

namespace RM {

class MatchSession;

// 模拟比赛和未来真实比赛数据共同实现的最小接口。
// 页面只依赖 MatchSession，不依赖 MQTT、JSON 或其他传输实现。
class MatchDataSource {
public:
    virtual ~MatchDataSource() = default;

    virtual MatchSession *matchSession() const = 0;
    virtual QString sourceKind() const = 0;
    virtual QString sourceName() const = 0;
    virtual bool isAvailable() const = 0;
    virtual QString statusText() const = 0;
};

} // namespace RM
