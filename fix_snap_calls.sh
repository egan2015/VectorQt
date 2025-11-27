#!/bin/bash

# 修复drawing-tool-bezier.cpp中的alignToGrid调用
sed -i '' 's/drawingScene->alignToGrid(m_controlPoints\[\([^]]*\)\])/drawingScene->snapManager() ? drawingScene->snapManager()->alignToGrid(m_controlPoints[\1]) : m_controlPoints[\1]/g' src/tools/drawing-tool-bezier.cpp

# 修复drawing-tool-node-edit.cpp中的类型引用
sed -i '' 's/DrawingScene::SnapResult/SnapResult/g' src/tools/drawing-tool-node-edit.cpp
sed -i '' 's/DrawingScene::ObjectSnapResult/ObjectSnapResult/g' src/tools/drawing-tool-node-edit.cpp

# 修复drawing-tool-outline-preview.cpp中的类型引用
sed -i '' 's/DrawingScene::SnapResult/SnapResult/g' src/tools/drawing-tool-outline-preview.cpp
sed -i '' 's/DrawingScene::ObjectSnapResult/ObjectSnapResult/g' src/tools/drawing-tool-outline-preview.cpp

echo "Fixed snap calls in tool files"
