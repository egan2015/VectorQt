#include <QPointF>
#include <QDebug>
#include "fastpathparser.h"

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
        qreal rx = qAbs(numbers[numIndex]);     // 确保半径为正
        qreal ry = qAbs(numbers[numIndex + 1]); // 确保半径为正
        qreal xAxisRotation = numbers[numIndex + 2];
        int largeArcFlag = (int)qRound(numbers[numIndex + 3]);
        int sweepFlag = (int)qRound(numbers[numIndex + 4]);
        qreal x = numbers[numIndex + 5];
        qreal y = numbers[numIndex + 6];

        if (isRelative) {
            x += currentPoint.x();
            y += currentPoint.y();
        }

        QPointF startPoint = currentPoint;
        QPointF endPoint(x, y);
        
        // 检查半径是否为零 - 如果是，则绘制直线
        if (rx == 0.0 || ry == 0.0) {
            path.lineTo(endPoint);
            currentPoint = endPoint;
            lastControlPoint = currentPoint;
            numIndex += 7;
            continue;
        }

        // 如果起点和终点相同，则不绘制任何内容
        if (qFuzzyCompare(startPoint.x(), endPoint.x()) && qFuzzyCompare(startPoint.y(), endPoint.y())) {
            currentPoint = endPoint;
            lastControlPoint = currentPoint;
            numIndex += 7;
            continue;
        }

        // SVG弧参数化实现，严格按照SVG规范
        // 1. 角度转换为弧度
        qreal phi = qDegreesToRadians(xAxisRotation);
        qreal cos_phi = qCos(phi);
        qreal sin_phi = qSin(phi);
        
        // 2. 将终点坐标变换到与椭圆对齐的坐标系
        qreal dx2 = (startPoint.x() - endPoint.x()) / 2.0;
        qreal dy2 = (startPoint.y() - endPoint.y()) / 2.0;
        qreal x1p = cos_phi * dx2 + sin_phi * dy2;
        qreal y1p = -sin_phi * dx2 + cos_phi * dy2;
        
        // 3. 确保半径足够大
        qreal rx_sq = rx * rx;
        qreal ry_sq = ry * ry;
        qreal x1p_sq = x1p * x1p;
        qreal y1p_sq = y1p * y1p;
        
        qreal lambda = (x1p_sq / rx_sq) + (y1p_sq / ry_sq);
        if (lambda > 1) {
            qreal lambda_sqrt = qSqrt(lambda);
            rx *= lambda_sqrt;
            ry *= lambda_sqrt;
            rx_sq = rx * rx;
            ry_sq = ry * ry;
        }
        
        // 4. 计算椭圆中心
        qreal sign = (largeArcFlag == sweepFlag) ? -1.0 : 1.0;
        qreal sq = ((rx_sq * ry_sq) - (rx_sq * y1p_sq) - (ry_sq * x1p_sq)) / 
                   ((rx_sq * y1p_sq) + (ry_sq * x1p_sq));
        sq = (sq < 0) ? 0 : sq;
        qreal coef = sign * qSqrt(sq);
        qreal cxp = coef * ((rx * y1p) / ry);
        qreal cyp = coef * (-(ry * x1p) / rx);
        
        // 椭圆中心在原始坐标系中的坐标
        qreal cx = ((startPoint.x() + endPoint.x()) / 2.0) + 
                   (cos_phi * cxp - sin_phi * cyp);
        qreal cy = ((startPoint.y() + endPoint.y()) / 2.0) + 
                   (sin_phi * cxp + cos_phi * cyp);
        
        // 5. 计算起始角度和扫掠角度
        QPointF vec1((x1p - cxp) / rx, (y1p - cyp) / ry);
        QPointF vec2((-x1p - cxp) / rx, (-y1p - cyp) / ry);
        qreal theta1 = qAtan2(vec1.y(), vec1.x());
        qreal delta_theta = qAtan2(vec2.y(), vec2.x()) - theta1;
        
        // 根据large-arc-flag和sweep-flag调整角度
        if (largeArcFlag == 0 && qAbs(delta_theta) > M_PI) {
            // 小弧情况，但角度差超过180度，需要调整
            delta_theta += (delta_theta > 0) ? -2 * M_PI : 2 * M_PI;
        } else if (largeArcFlag == 1 && qAbs(delta_theta) < M_PI) {
            // 大弧情况，但角度差小于180度，需要调整
            delta_theta += (delta_theta > 0) ? -2 * M_PI : 2 * M_PI;
        }
        
        if (sweepFlag == 0 && delta_theta > 0) {
            // sweep-flag为0表示逆时针，但角度差为正，需要调整
            delta_theta -= 2 * M_PI;
        } else if (sweepFlag == 1 && delta_theta < 0) {
            // sweep-flag为1表示顺时针，但角度差为负，需要调整
            delta_theta += 2 * M_PI;
        }
        
        // 6. 使用Qt的arcTo方法绘制椭圆弧
        // 计算椭圆边界矩形
        QRectF ellipseRect(cx - rx, cy - ry, 2 * rx, 2 * ry);
        
        // 计算起始角度和扫掠角度（转换为度数）
        qreal startAngle = qRadiansToDegrees(theta1);
        qreal spanAngle = qRadiansToDegrees(delta_theta);
        
        // SVG和Qt在角度方向上的差异：
        // SVG：sweepFlag=0表示逆时针，sweepFlag=1表示顺时针
        // Qt：正角度表示逆时针，负角度表示顺时针
        
        // 根据SVG的sweepFlag调整Qt的角度方向
        if (sweepFlag == 0) {
            // SVG逆时针，Qt正角度
            // spanAngle保持不变
        } else {
            // SVG顺时针，Qt负角度
            spanAngle = -spanAngle;
        }
        
        // 对于旋转椭圆，Qt的arcTo不能直接处理旋转
        // 我们需要调整起始角度以考虑旋转
        if (!qFuzzyIsNull(xAxisRotation)) {
            // 对于旋转椭圆，调整起始角度
            startAngle -= xAxisRotation;
        }
        
        // 直接使用arcTo绘制弧线
        path.arcTo(ellipseRect, startAngle, spanAngle);
        
        currentPoint = endPoint;
        lastControlPoint = currentPoint;

        numIndex += 7;
    }
}