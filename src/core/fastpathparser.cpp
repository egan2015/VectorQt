#include "fastpathparser.h"
#include <QPointF>
#include <QDebug>

void FastPathParser::parsePathData(const QString &data, QPainterPath &path)
{
    if (data.isEmpty()) return;

    // 预分配空间，减少重新分配
    QVector<qreal> numbers;
    numbers.reserve(64);

    QPointF currentPoint(0, 0);
    QPointF pathStart(0, 0);
    QPointF lastControlPoint(0, 0);

    const QChar *chars = data.constData();
    const int length = data.length();
    int pos = 0;

    while (pos < length) {
        // 跳过空白字符
        while (pos < length && chars[pos].isSpace()) {
            pos++;
        }

        if (pos >= length) break;

        QChar cmd = chars[pos];
        if (cmd.isLetter()) {
            // 处理命令
            bool isRelative = cmd.isLower();
            cmd = cmd.toUpper();
            pos++;

            // 解析数字参数
            numbers.clear();
            while (pos < length) {
                // 跳过逗号和空白
                while (pos < length && (chars[pos] == ',' || chars[pos].isSpace())) {
                    pos++;
                }
                if (pos >= length || chars[pos].isLetter()) break;

                // 快速解析数字
                int start = pos;
                while (pos < length && (chars[pos].isDigit() || chars[pos] == '.' || 
                       chars[pos] == '-' || chars[pos] == '+' || chars[pos].toLower() == 'e')) {
                    pos++;
                }

                if (start < pos) {
                    numbers.append(fastParseDouble(data, start, pos));
                } else {
                    pos++; // 防止死循环
                }
            }

            // 执行命令
            executeCommand(cmd, numbers, isRelative, path, currentPoint, pathStart, lastControlPoint);
        } else {
            pos++; // 跳过非命令字符
        }
    }
}

qreal FastPathParser::fastParseDouble(const QString &str, int start, int end)
{
    // 简化的快速数字解析，避免QString::toDouble的开销
    bool negative = false;
    qreal result = 0.0;
    qreal fraction = 0.0;
    qreal divisor = 1.0;
    bool hasFraction = false;
    bool hasExponent = false;
    int exponentSign = 1;
    int exponent = 0;

    int i = start;
    
    // 处理符号
    if (i < end && (str[i] == '-' || str[i] == '+')) {
        negative = (str[i] == '-');
        i++;
    }

    // 处理整数部分
    while (i < end && str[i].isDigit()) {
        result = result * 10.0 + (str[i].toLatin1() - '0');
        i++;
    }

    // 处理小数部分
    if (i < end && str[i] == '.') {
        hasFraction = true;
        i++;
        while (i < end && str[i].isDigit()) {
            fraction = fraction * 10.0 + (str[i].toLatin1() - '0');
            divisor *= 10.0;
            i++;
        }
        result += fraction / divisor;
    }

    // 处理指数部分
    if (i < end && (str[i] == 'e' || str[i] == 'E')) {
        hasExponent = true;
        i++;
        if (i < end && (str[i] == '-' || str[i] == '+')) {
            exponentSign = (str[i] == '-') ? -1 : 1;
            i++;
        }
        while (i < end && str[i].isDigit()) {
            exponent = exponent * 10 + (str[i].toLatin1() - '0');
            i++;
        }
    }

    // 应用指数
    if (hasExponent) {
        for (int j = 0; j < exponent; j++) {
            if (exponentSign > 0) {
                result *= 10.0;
            } else {
                result /= 10.0;
            }
        }
    }

    return negative ? -result : result;
}

void FastPathParser::executeCommand(QChar cmd, const QVector<qreal> &numbers, bool isRelative,
                                  QPainterPath &path, QPointF &currentPoint, 
                                  QPointF &pathStart, QPointF &lastControlPoint)
{
    const int numCount = numbers.size();
    int numIndex = 0;

    switch (cmd.toLatin1()) {
    case 'M': // 移动到
        while (numIndex + 1 < numCount) {
            qreal x = numbers[numIndex];
            qreal y = numbers[numIndex + 1];
            if (isRelative) {
                x += currentPoint.x();
                y += currentPoint.y();
            }
            path.moveTo(x, y);
            currentPoint = QPointF(x, y);
            pathStart = currentPoint;
            lastControlPoint = currentPoint;
            numIndex += 2;
        }
        break;

    case 'L': // 线条到
        while (numIndex + 1 < numCount) {
            qreal x = numbers[numIndex];
            qreal y = numbers[numIndex + 1];
            if (isRelative) {
                x += currentPoint.x();
                y += currentPoint.y();
            }
            path.lineTo(x, y);
            currentPoint = QPointF(x, y);
            lastControlPoint = currentPoint;
            numIndex += 2;
        }
        break;

    case 'H': // 水平线
        while (numIndex < numCount) {
            qreal x = numbers[numIndex];
            if (isRelative) {
                x += currentPoint.x();
            }
            path.lineTo(x, currentPoint.y());
            currentPoint.setX(x);
            lastControlPoint = currentPoint;
            numIndex += 1;
        }
        break;

    case 'V': // 垂直线
        while (numIndex < numCount) {
            qreal y = numbers[numIndex];
            if (isRelative) {
                y += currentPoint.y();
            }
            path.lineTo(currentPoint.x(), y);
            currentPoint.setY(y);
            lastControlPoint = currentPoint;
            numIndex += 1;
        }
        break;

    case 'C': // 三次贝塞尔曲线
    case 'S': // 平滑三次贝塞尔曲线
    case 'Q': // 二次贝塞尔曲线
    case 'T': // 平滑二次贝塞尔曲线
        parseBezierCommands(cmd, numbers, isRelative, path, currentPoint, lastControlPoint);
        break;

    case 'A': // 椭圆弧
        parseArcCommand(numbers, isRelative, path, currentPoint, pathStart, lastControlPoint);
        break;

    case 'Z': // 闭合路径
        path.closeSubpath();
        currentPoint = pathStart;
        lastControlPoint = currentPoint;
        break;

    default:
        // 未知命令，忽略
        break;
    }
}

void FastPathParser::parseBezierCommands(QChar cmd, const QVector<qreal> &numbers, bool isRelative,
                                        QPainterPath &path, QPointF &currentPoint, 
                                        QPointF &lastControlPoint)
{
    const int numCount = numbers.size();
    int numIndex = 0;

    switch (cmd.toLatin1()) {
    case 'C': // 三次贝塞尔曲线
        while (numIndex + 5 < numCount) {
            qreal cp1x = numbers[numIndex];
            qreal cp1y = numbers[numIndex + 1];
            qreal cp2x = numbers[numIndex + 2];
            qreal cp2y = numbers[numIndex + 3];
            qreal x = numbers[numIndex + 4];
            qreal y = numbers[numIndex + 5];

            if (isRelative) {
                cp1x += currentPoint.x();
                cp1y += currentPoint.y();
                cp2x += currentPoint.x();
                cp2y += currentPoint.y();
                x += currentPoint.x();
                y += currentPoint.y();
            }

            path.cubicTo(cp1x, cp1y, cp2x, cp2y, x, y);
            lastControlPoint = QPointF(cp2x, cp2y);
            currentPoint = QPointF(x, y);
            numIndex += 6;
        }
        break;

    case 'S': // 平滑三次贝塞尔曲线
        while (numIndex + 3 < numCount) {
            qreal cp2x = numbers[numIndex];
            qreal cp2y = numbers[numIndex + 1];
            qreal x = numbers[numIndex + 2];
            qreal y = numbers[numIndex + 3];

            if (isRelative) {
                cp2x += currentPoint.x();
                cp2y += currentPoint.y();
                x += currentPoint.x();
                y += currentPoint.y();
            }

            // 计算第一个控制点（前一个控制点的反射）
            qreal cp1x = currentPoint.x() + (currentPoint.x() - lastControlPoint.x());
            qreal cp1y = currentPoint.y() + (currentPoint.y() - lastControlPoint.y());

            path.cubicTo(cp1x, cp1y, cp2x, cp2y, x, y);
            lastControlPoint = QPointF(cp2x, cp2y);
            currentPoint = QPointF(x, y);
            numIndex += 4;
        }
        break;

    case 'Q': // 二次贝塞尔曲线
        while (numIndex + 3 < numCount) {
            qreal cpx = numbers[numIndex];
            qreal cpy = numbers[numIndex + 1];
            qreal x = numbers[numIndex + 2];
            qreal y = numbers[numIndex + 3];

            if (isRelative) {
                cpx += currentPoint.x();
                cpy += currentPoint.y();
                x += currentPoint.x();
                y += currentPoint.y();
            }

            path.quadTo(cpx, cpy, x, y);
            lastControlPoint = QPointF(cpx, cpy);
            currentPoint = QPointF(x, y);
            numIndex += 4;
        }
        break;

    case 'T': // 平滑二次贝塞尔曲线
        while (numIndex + 1 < numCount) {
            qreal x = numbers[numIndex];
            qreal y = numbers[numIndex + 1];

            if (isRelative) {
                x += currentPoint.x();
                y += currentPoint.y();
            }

            // 计算控制点（前一个控制点的反射）
            qreal cpx = currentPoint.x() + (currentPoint.x() - lastControlPoint.x());
            qreal cpy = currentPoint.y() + (currentPoint.y() - lastControlPoint.y());

            path.quadTo(cpx, cpy, x, y);
            lastControlPoint = QPointF(cpx, cpy);
            currentPoint = QPointF(x, y);
            numIndex += 2;
        }
        break;
    }
}

void FastPathParser::parseArcCommand(const QVector<qreal> &numbers, bool isRelative,
                                   QPainterPath &path, QPointF &currentPoint, 
                                   QPointF &pathStart, QPointF &lastControlPoint)
{
    const int numCount = numbers.size();
    if (numCount < 7) return; // A命令至少需要7个参数
    
    int numIndex = 0;
    while (numIndex + 6 < numCount) {
        qreal rx = numbers[numIndex];
        qreal ry = numbers[numIndex + 1];
        qreal xAxisRotation = numbers[numIndex + 2];
        qreal largeArcFlag = numbers[numIndex + 3];
        qreal sweepFlag = numbers[numIndex + 4];
        qreal x = numbers[numIndex + 5];
        qreal y = numbers[numIndex + 6];

        if (isRelative) {
            x += currentPoint.x();
            y += currentPoint.y();
        }

        // 使用Qt内置的arcTo方法，简化实现
        QRectF boundingRect(currentPoint.x() - rx, currentPoint.y() - ry, rx * 2, ry * 2);
        
        // 计算椭圆弧的参数
        qreal startAngle = 0;
        qreal spanAngle = 360.0;
        
        // 通过计算起点和终点来确定角度
        QPointF startPoint = currentPoint;
        QPointF endPoint = QPointF(x, y);
        
        // 将椭圆中心移动到原点进行计算
        QPointF center = startPoint + QPointF(rx, ry);
        QPointF startPointFromCenter = startPoint - center;
        QPointF endPointFromCenter = endPoint - center;
        
        // 计算起点角度
        startAngle = qAtan2(startPointFromCenter.y() / ry, startPointFromCenter.x() / rx) * 180.0 / M_PI;
        
        // 计算终点角度
        qreal endAngle = qAtan2(endPointFromCenter.y() / ry, endPointFromCenter.x() / rx) * 180.0 / M_PI;
        
        // 计算角度差
        spanAngle = endAngle - startAngle;
        
        // 处理large-arc-flag
        if (largeArcFlag) {
            if (qAbs(spanAngle) < 180) {
                spanAngle = spanAngle < 0 ? spanAngle + 360 : spanAngle - 360;
            }
        } else {
            if (qAbs(spanAngle) > 180) {
                spanAngle = spanAngle < 0 ? spanAngle + 360 : spanAngle - 360;
            }
        }
        
        // 处理sweep-flag
        if (!sweepFlag && spanAngle > 0) {
            spanAngle -= 360;
        } else if (sweepFlag && spanAngle < 0) {
            spanAngle += 360;
        }
        
        // 应用x轴旋转
        if (xAxisRotation != 0) {
            QTransform transform;
            transform.translate(center.x(), center.y());
            transform.rotate(xAxisRotation);
            boundingRect = transform.mapRect(boundingRect);
            transform.translate(-center.x(), -center.y());
        }
        
        // 添加椭圆弧到路径
        path.arcTo(boundingRect, startAngle, spanAngle);
        
        currentPoint = endPoint;
        lastControlPoint = currentPoint;

        numIndex += 7;
    }
}