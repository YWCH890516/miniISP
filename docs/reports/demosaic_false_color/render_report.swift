import AppKit
import CoreText
import PDFKit

let dir = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
let content = try JSONSerialization.jsonObject(with: Data(contentsOf: dir.appendingPathComponent("report_content.json"))) as! [String: Any]
let pages = content["pages"] as! [[String: Any]]
let references = content["references"] as! [[String]]
let referenceURLs = Dictionary(uniqueKeysWithValues: references.map { ($0[0], $0[3]) })
let output = dir.appendingPathComponent("demosaic_false_color_zh-TW.pdf")
let W: CGFloat = 595.276, H: CGFloat = 841.89, M: CGFloat = 42
let CW: CGFloat = W - 2 * M
let regular = "PingFangTC-Regular", bold = "PingFangTC-Semibold"
func color(_ hex: UInt32) -> NSColor {
    NSColor(srgbRed: CGFloat(hex >> 16 & 255)/255, green: CGFloat(hex >> 8 & 255)/255, blue: CGFloat(hex & 255)/255, alpha: 1)
}
let ink = color(0x20313D), blue = color(0x245DA6), muted = color(0x5F6E75)
let pale = color(0xEAF1F4), border = color(0xD5DFE3)
var media = CGRect(x: 0, y: 0, width: W, height: H)
let ctx = CGContext(consumer: CGDataConsumer(url: output as CFURL)!, mediaBox: &media,
    [kCGPDFContextTitle: "Demosaic 偽色抑制｜miniISP 技術報告",
     kCGPDFContextAuthor: "miniISP project notes",
     kCGPDFContextSubject: "公開方法、重建原理、色度後處理與可重現驗證"] as CFDictionary)!
var pageIndex = 0
var links = [(Int, CGRect, String)]()
var textFailures = [String]()

func rect(_ x: CGFloat, _ y: CGFloat, _ w: CGFloat, _ h: CGFloat, _ fill: NSColor) {
    ctx.setFillColor(fill.cgColor)
    ctx.fill(CGRect(x: x, y: H-y-h, width: w, height: h))
}
func attributed(_ s: String, _ size: CGFloat, _ strong: Bool = false, _ c: NSColor = ink, _ mono: Bool = false, _ spacing: CGFloat = 2.6) -> NSAttributedString {
    let style = NSMutableParagraphStyle()
    style.lineSpacing = spacing
    style.lineBreakMode = .byWordWrapping
    let font = NSFont(name: mono ? "Menlo-Regular" : strong ? bold : regular, size: size)!
    let a = NSMutableAttributedString(string: s, attributes: [.font: font, .foregroundColor: c, .paragraphStyle: style])
    for match in try! NSRegularExpression(pattern: "\\[([1-7])\\]").matches(in: s, range: NSRange(location: 0, length: (s as NSString).length)) {
        a.addAttributes([.foregroundColor: blue], range: match.range)
    }
    return a
}
func measure(_ s: String, _ width: CGFloat, _ size: CGFloat, _ strong: Bool = false, _ mono: Bool = false, _ spacing: CGFloat = 2.6) -> CGFloat {
    let a = attributed(s, size, strong, ink, mono, spacing)
    let z = CTFramesetterSuggestFrameSizeWithConstraints(CTFramesetterCreateWithAttributedString(a), CFRange(location: 0, length: 0), nil, CGSize(width: width, height: 10000), nil)
    return ceil(z.height) + 3
}
@discardableResult
func text(_ s: String, _ x: CGFloat, _ y: CGFloat, _ width: CGFloat, _ size: CGFloat = 10.2, _ strong: Bool = false, _ c: NSColor = ink, _ mono: Bool = false, _ spacing: CGFloat = 2.6) -> CGFloat {
    let a = attributed(s, size, strong, c, mono, spacing)
    let ht = measure(s, width, size, strong, mono, spacing)
    let bounds = CGRect(x: x, y: H-y-ht, width: width, height: ht)
    let frame = CTFramesetterCreateFrame(CTFramesetterCreateWithAttributedString(a), CFRange(location: 0, length: 0), CGPath(rect: bounds, transform: nil), nil)
    if CTFrameGetVisibleStringRange(frame).length != a.length { textFailures.append(s) }
    ctx.textMatrix = .identity
    CTFrameDraw(frame, ctx)
    let ctLines = CTFrameGetLines(frame) as! [CTLine]
    var origins = Array(repeating: CGPoint.zero, count: ctLines.count)
    CTFrameGetLineOrigins(frame, CFRange(location: 0, length: 0), &origins)
    let ns = s as NSString
    for match in try! NSRegularExpression(pattern: "\\[([1-7])\\]").matches(in: s, range: NSRange(location: 0, length: ns.length)) {
        guard let url = referenceURLs[ns.substring(with: match.range(at: 1))] else { continue }
        for (idx, line) in ctLines.enumerated() {
            let r = CTLineGetStringRange(line)
            if match.range.location >= r.location && match.range.location < r.location+r.length {
                let start = CTLineGetOffsetForStringIndex(line, match.range.location, nil)
                let end = CTLineGetOffsetForStringIndex(line, NSMaxRange(match.range), nil)
                links.append((pageIndex, CGRect(x: x+origins[idx].x+start, y: bounds.minY+origins[idx].y-3, width: max(12,end-start), height: size+6), url))
            }
        }
    }
    return ht
}
func drawImage(_ path: String, _ x: CGFloat, _ y: CGFloat, _ width: CGFloat, _ height: CGFloat) {
    let image = NSImage(contentsOf: dir.appendingPathComponent(path))!
    let cg = image.cgImage(forProposedRect: nil, context: nil, hints: nil)!
    let scale = min(width/CGFloat(cg.width), height/CGFloat(cg.height))
    let w = CGFloat(cg.width)*scale, h = CGFloat(cg.height)*scale
    rect(x, y, width, height, color(0xF1F3F3))
    ctx.interpolationQuality = .none
    ctx.draw(cg, in: CGRect(x: x+(width-w)/2, y: H-y-(height+h)/2, width: w, height: h))
}

func block(_ b: [String: Any], _ y: CGFloat, compact: Bool) -> CGFloat {
    let size: CGFloat = compact ? 9.4 : 10.2
    switch b["type"] as! String {
    case "p":
        let refs = (b["refs"] as? [String] ?? []).map { "[\($0)]" }.joined()
        let s = (b["text"] as! String) + (refs.isEmpty ? "" : " " + refs)
        return text(s, M, y, CW, size) + (compact ? 7 : 10)
    case "h":
        return text(b["text"] as! String, M, y+3, CW, 12.1, true, blue) + 12
    case "callout":
        let s = b["text"] as! String
        let ht = measure(s, CW-26, size, true) + 20
        rect(M, y, CW, ht, pale); rect(M, y, 3, ht, blue)
        text(s, M+13, y+10, CW-26, size, true)
        return ht + 11
    case "code":
        let s = b["text"] as! String
        let ht = measure(s, CW-24, 9, false, true, 3) + 19
        rect(M, y, CW, ht, color(0xF1F4F5))
        text(s, M+12, y+9, CW-24, 9, false, ink, true, 3)
        return ht + 11
    case "flow":
        let items = b["items"] as! [String]
        let gap: CGFloat = 15, w = (CW-gap*CGFloat(items.count-1))/CGFloat(items.count)
        let ht: CGFloat = 43
        for (i, s) in items.enumerated() {
            let x = M+CGFloat(i)*(w+gap)
            rect(x, y, w, ht, i == 1 ? color(0xDCE8F3) : pale)
            text(s, x+7, y+13, w-14, 9.1, true, blue)
            if i < items.count-1 { text("→", x+w+2, y+13, gap-2, 10.2, true, blue) }
        }
        return ht + 15
    case "table":
        let heads = b["headers"] as! [String], rows = b["rows"] as! [[String]]
        let ratios = b["widths"] as? [Double] ?? Array(repeating: 1.0/Double(heads.count), count: heads.count)
        let widths = ratios.map { CGFloat($0)*CW }
        var top = y
        for (rowIndex,row) in ([heads] + rows).enumerated() {
            let strong = rowIndex == 0, font: CGFloat = strong ? 9.0 : (compact ? 8.5 : 9.0)
            let ht = zip(row,widths).map { measure($0, $1-12, font, strong, false, 2) }.max()! + 12
            rect(M, top, CW, ht, strong ? color(0xDCE7ED) : rowIndex%2 == 0 ? color(0xF3F6F6) : .white)
            var x = M
            for (s,w) in zip(row,widths) { text(s, x+6, top+6, w-12, font, strong, ink, false, 2); x += w }
            rect(M, top+ht-0.5, CW, 0.5, border)
            top += ht
        }
        return top-y+12
    case "images":
        let items = b["items"] as! [[String: String]]
        let gap: CGFloat = 10, width = (CW-gap*CGFloat(items.count-1))/CGFloat(items.count)
        let ht = CGFloat(b["height"] as! Double)
        for (i,item) in items.enumerated() {
            let x = M+CGFloat(i)*(width+gap)
            text(item["label"]!, x, y, width, 9, true, blue)
            drawImage(item["file"]!, x, y+20, width, ht)
        }
        let cap = text(b["caption"] as! String, M, y+ht+26, CW, 8.5, false, muted, false, 2)
        return ht+26+cap+10
    case "mosaic":
        let cell: CGFloat = 27
        for row in 0..<4 {
            for col in 0..<4 {
                let c = row%2 == 0 ? (col%2 == 0 ? "R" : "G") : (col%2 == 0 ? "G" : "B")
                let fill = c == "R" ? color(0xEDC7C4) : c == "G" ? color(0xC7DED1) : color(0xC4D4ED)
                let x = M+CGFloat(col)*cell, top = y+CGFloat(row)*cell
                rect(x, top, cell-1, cell-1, fill)
                text(c, x+9, top+5, 18, 10.3, true)
            }
        }
        text("不同位置量到不同通道", M, y+116, 150, 9, true, blue)
        text("→", M+126, y+40, 24, 22, true, blue)
        text("假設原本是中性細線", M+165, y+3, CW-165, 11.2, true)
        text("理想：R = 150，G = 150，B = 150\n重建：R = 120，G = 180，B = 130\n通道估計誤差不一致 → 看起來偏綠", M+165, y+29, CW-165, 10.2)
        text("右側數字為機制示意，並非 Cat 圖的量測。", M+165, y+105, CW-165, 8.6, false, muted)
        return 143
    case "reference":
        let title = "[\(b["number"]!)] \(b["author"]!)"
        var ht = text(title, M, y, CW, 9.1, true, blue)
        let t = b["text"] as! String
        let titleHeight = text(t, M+16, y+ht+2, CW-16, 9.1)
        links.append((pageIndex, CGRect(x: M+16, y: H-y-ht-2-titleHeight, width: CW-16, height: titleHeight), b["url"] as! String))
        ht += titleHeight + 11
        return ht
    default: fatalError("Unknown block")
    }
}

var overfull = [String]()
for (i,page) in pages.enumerated() {
    pageIndex = i
    ctx.beginPDFPage(nil)
    rect(0, 0, W, H, .white)
    rect(M, 28, 20, 3, blue)
    text("miniISP  /  IMAGE SCIENCE NOTES", M+30, 22, CW-30, 8, true, muted)
    let titleHeight = text(page["title"] as! String, M, 47, CW, i == 0 ? 27 : 21.5, true)
    let subtitleY = 47+titleHeight+5
    let subHeight = text(page["subtitle"] as! String, M, subtitleY, CW, 9, false, muted)
    var y = subtitleY+subHeight+17
    rect(M, y-8, CW, 0.7, border)
    let compact = i == 5
    for item in page["blocks"] as! [[String: Any]] { y += block(item, y, compact: compact) }
    if y > 787 { overfull.append("Page \(i+1): bottom \(Int(y))") }
    print("Page \(i+1): body ends at \(Int(y)) / 787 pt")
    rect(M, 800, CW, 0.6, border)
    text("miniISP｜偽色抑制技術報告｜2026-11-15", M, 811, CW-65, 7.5, false, muted)
    text(String(format: "%02d / %02d", i+1, pages.count), W-M-50, 811, 55, 7.5, false, muted)
    ctx.endPDFPage()
}
ctx.closePDF()
precondition(textFailures.isEmpty, "Truncated text: \(textFailures)")
if !overfull.isEmpty { print("LAYOUT OVERFLOW: \(overfull.joined(separator: "; "))") }

let pdf = PDFDocument(url: output)!
for (page,bounds,url) in links {
    let a = PDFAnnotation(bounds: bounds, forType: .link, withProperties: nil)
    a.action = PDFActionURL(url: URL(string: url)!)
    a.border = PDFBorder(); a.border!.lineWidth = 0
    pdf.page(at: page)!.addAnnotation(a)
}
precondition(pdf.write(to: output))
let saved = PDFDocument(url: output)!
precondition(saved.pageCount == pages.count)
// CoreText may map CJK glyphs to compatibility radicals in extracted PDF text.
let searchableText = (saved.string ?? "").precomposedStringWithCompatibilityMapping
for keyword in ["偽色", "色差", "MHC", "AHD", "miniISP", "0.10489"] {
    precondition(searchableText.contains(keyword), "Missing searchable text: \(keyword)")
}
try searchableText.write(to: dir.appendingPathComponent("extracted_text.txt"), atomically: true, encoding: .utf8)
let proofs = dir.appendingPathComponent("preview")
try FileManager.default.createDirectory(at: proofs, withIntermediateDirectories: true)
var thumbnails = [CGImage]()
for i in 0..<saved.pageCount {
    let scale: CGFloat = 1.6, w = Int(W*scale), h = Int(H*scale)
    let bitmap = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: w, pixelsHigh: h,
        bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
        colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
    let graphics = NSGraphicsContext(bitmapImageRep: bitmap)!
    NSGraphicsContext.saveGraphicsState(); NSGraphicsContext.current = graphics
    let cg = graphics.cgContext
    cg.setFillColor(NSColor.white.cgColor); cg.fill(CGRect(x: 0, y: 0, width: w, height: h))
    cg.scaleBy(x: scale, y: scale)
    saved.page(at: i)!.draw(with: .mediaBox, to: cg)
    NSGraphicsContext.restoreGraphicsState()
    try bitmap.representation(using: .png, properties: [:])!.write(to: proofs.appendingPathComponent(String(format: "page_%02d.png", i+1)))
    thumbnails.append(bitmap.cgImage!)
}
let columns = 3, rows = (pages.count+2)/3
let tw = 340, th = Int(CGFloat(tw)*H/W), gap = 12
let sw = columns*tw+(columns+1)*gap, sh = rows*th+(rows+1)*gap
let sheet = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: sw, pixelsHigh: sh, bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
let graphics = NSGraphicsContext(bitmapImageRep: sheet)!
NSGraphicsContext.saveGraphicsState(); NSGraphicsContext.current = graphics
let cg = graphics.cgContext
cg.setFillColor(color(0xDCE2E3).cgColor); cg.fill(CGRect(x: 0, y: 0, width: sw, height: sh))
for (i,im) in thumbnails.enumerated() {
    let x = gap+(i%columns)*(tw+gap), y = sh-gap-(i/columns+1)*th-(i/columns)*gap
    cg.draw(im, in: CGRect(x: x, y: y, width: tw, height: th))
}
NSGraphicsContext.restoreGraphicsState()
try sheet.representation(using: .png, properties: [:])!.write(to: dir.appendingPathComponent("report_overview.png"))
precondition(overfull.isEmpty, "Fix overflowing pages before delivery")
print("Verified \(pages.count) A4 pages, searchable Chinese text, and \(links.count) source links.")
print(output.path)
