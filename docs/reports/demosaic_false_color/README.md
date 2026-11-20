# Demosaic 偽色抑制報告

主要交付：[中文 PDF](demosaic_false_color_zh-TW.pdf)，共 9 頁 A4，文字可選取，來源編號及參考文獻標題可點擊。

- [報告總覽](report_overview.png)
- [可編輯 Markdown](demosaic_false_color_zh-TW.md)
- [合成測試數據](data/metrics.csv)
- [輸入與程式版本雜湊](manifest.json)

內容涵蓋偽色／摩爾紋／zipper 的差異、方向與色差插值、MHC／AHD／RI 方法比較、色度後處理、合成控制實驗，以及目前 miniISP 的程式現況與下一版驗證規劃。

## 實驗範圍

`experiment.cpp` 直接呼叫目前的 `src/Demosaic.cpp` 與 `src/BayerUtils.cpp`，沒有變更核心演算法。合成資料為 128×96、RGGB，16-bit 量化、black=0、white=65535，不含雜訊、光學模糊、AWB 或 CCM。

對照組是原有 `DemosaicBilinear16`；處理組是在其輸出上，保持 G 不變，對 signed R−G、B−G 各做一次 3×3 median，再加回 G。這只是獨立示範，不是已加入正式 ISP 的功能，也不是 AHD／RI 或 IPOL 程式碼的完整重現。

四張圖樣：均勻灰、黑白斜邊、中性斜向正弦紋理、真正的紅／藍細線。每張的參考值包含相同 16-bit 量化；輸出數據以 float/double 保存，數值指標在最終 clipping 及 sRGB 編碼前計算，四邊各排除 3 pixels。測試中輸出沒有超出 [0,1] 的通道樣本。

指標：

- `rgb_rmse_linear`：所有 RGB 通道的線性 RMSE，參考範圍 [0,1]。
- `chroma_error_linear`：mean((abs((R−G)out−(R−G)ref) + abs((B−G)out−(B−G)ref))/2)。對中性參考它就是通道不一致程度；對彩色參考則必須保留參考色差，不能把低飽和度當成好結果。
- `green_mae_linear`：G 通道的線性 MAE。本示範不改 G，因此前後相同；它不是標準 luminance 或解析度指標。
- `out_of_range_channel_pct`：裁切前，評估區域內超出 [0,1] 的通道樣本占比。

影像用同一條 sRGB 曲線產生 8-bit 預覽，PDF 使用最近鄰顯示合成圖樣。這是理想化數位取樣測試，不是實際 sensor 的光譜／光學模擬。

## 本地圖片

Cat 裁切來自 `Image/ReportImage/CatYesBLCYesAWBYessRGB.ppm`，在 RAW 方向的零起算座標 (700,150) 和 (1150,1700)，各 512×384。只做裁切，沒有追加調色、降噪或增銳；報告中縮放顯示，不能據此保證原圖沒有微小偽色。

Tree 的 PSNR 引用先前 `docs/career/comparisons/data/metrics.csv`；該測試從 8-bit 編碼 RGB 合成 RGGB，和此次 16-bit 線性測試的定義不同，不能把分數混在一起排名。

## 重現

在專案根目錄執行。以下路徑對應此台 macOS 的現有工具鏈；MacOSX26.5 SDK 用於避開預設 SDK 與 Swift 工具鏈版本不相容。

```bash
/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++ \
  -std=c++17 -O2 -Wall -Wextra -Wpedantic \
  -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk \
  -Iinclude \
  docs/reports/demosaic_false_color/experiment.cpp \
  src/Demosaic.cpp src/BayerUtils.cpp \
  -o /private/tmp/miniisp_false_color_experiment

/private/tmp/miniisp_false_color_experiment

python3 docs/reports/demosaic_false_color/build_report.py

swift \
  -sdk /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk \
  -module-cache-path /private/tmp/miniisp-pdf-module-cache \
  docs/reports/demosaic_false_color/render_report.swift
```

Python 僅使用標準函式庫；PDF 使用 macOS CoreText／PDFKit。產生器檢查文字是否超出文字框、內文是否侵入頁尾、頁數及中文字擷取，並輸出 `preview/page_*.png` 供人工檢查。部分字型擷取會使用相容部首，因此 `extracted_text.txt` 經 Unicode NFKC 正規化。

## 使用與限制

公式、圖樣與摘要均重新撰寫，沒有複製第三方原始碼或論文圖表；七項來源見報告第 9 頁。方法表是公開方法的比較，不代表已執行 MHC、AHD 或 RI 的效能實驗，也不代表特定廠商的產品實作。

檔案皆新增在本資料夾，不更動 `main.cpp`、核心 `.cpp/.h` 或 `Image/` 內的原始圖片。
