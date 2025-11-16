#include <QDebug>
#include <QColor>
#include <QRegularExpression>

QColor parseColor(const QString &colorStr)
{
    if (colorStr.startsWith("#")) {
        return QColor(colorStr);
    } else if (colorStr.startsWith("rgb(")) {
        // 解析rgb(r,g,b)格式
        QRegularExpression regex(R"(rgb\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\))");
        QRegularExpressionMatch match = regex.match(colorStr);
        if (match.hasMatch()) {
            int r = match.captured(1).toInt();
            int g = match.captured(2).toInt();
            int b = match.captured(3).toInt();
            return QColor(r, g, b);
        }
    } else if (colorStr.startsWith("rgba(")) {
        // 解析rgba(r,g,b,a)格式
        QRegularExpression regex(R"(rgba\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*([\d.]+)\s*\))");
        QRegularExpressionMatch match = regex.match(colorStr);
        if (match.hasMatch()) {
            int r = match.captured(1).toInt();
            int g = match.captured(2).toInt();
            int b = match.captured(3).toInt();
            qreal a = match.captured(4).toDouble(); // a是0-1范围的透明度值
            return QColor(r, g, b, static_cast<int>(a * 255)); // 转换为0-255范围
        }
    }
    
    // 尝试直接使用颜色名称
    QColor color(colorStr);
    if (color.isValid()) {
        return color;
    }
    
    // 默认返回黑色
    return QColor(Qt::black);
}

int main()
{
    // 测试各种颜色格式
    QStringList testColors = {
        "red",
        "blue",
        "green",
        "#FF6B6B",
        "#4ECDC4",
        "#45B7D1",
        "rgb(255, 99, 71)",
        "rgb(78, 205, 196)",
        "rgb(69, 183, 209)",
        "rgba(255, 99, 71, 0.5)",
        "rgba(78, 205, 196, 0.7)",
        "rgba(69, 183, 209, 0.3)",
        "none"
    };
    
    for (const QString &colorStr : testColors) {
        QColor color = parseColor(colorStr);
        qDebug() << colorStr << "->" << color.name() << (color.isValid() ? "(valid)" : "(invalid)");
    }
    
    return 0;
}