// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

// Generates resources/icons/qsensors.svg from the app's own LCD segment font,
// so the icon always matches the in-app display. See tools/icon/README.md.
#include "lcd_segment_font.h"
#include <QFile>
#include <QTextStream>
#include <QTransform>

using namespace LcdSegmentFont;

static QString pts(const QPolygonF &p) {
    QStringList l;
    for (const QPointF &q: p) l << QString::asprintf("%.2f,%.2f", q.x(), q.y());
    return l.join(' ');
}

int main(int argc, char **argv) {
    const QString out = argc > 1 ? argv[1] : "qsensors.svg";
    QString svg;
    QTextStream s(&svg);
    // Canvas 256; device body, recessed LCD window, digits, bar graph.
    const QRectF win(28, 58, 200, 142);
    s << R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256">
<defs>
 <linearGradient id="body" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#4a5057"/><stop offset="1" stop-color="#25292e"/></linearGradient>
 <linearGradient id="lcd" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#b9c9ad"/><stop offset="0.3" stop-color="#cddbc2"/><stop offset="1" stop-color="#d8e4ce"/></linearGradient>
 <linearGradient id="gloss" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ffffff" stop-opacity="0.18"/><stop offset="1" stop-color="#ffffff" stop-opacity="0"/></linearGradient>
</defs>
<rect x="12" y="12" width="232" height="232" rx="46" fill="url(#body)"/>
<rect x="12.75" y="12.75" width="230.5" height="230.5" rx="45.5" fill="none" stroke="#ffffff" stroke-opacity="0.14" stroke-width="1.5"/>
<rect x="12" y="12" width="232" height="100" rx="46" fill="url(#gloss)"/>
)svg";
    s << QString(R"svg(<rect x="%1" y="%2" width="%3" height="%4" rx="14" fill="url(#lcd)" stroke="#15181b" stroke-width="3"/>
<rect x="%5" y="%6" width="%7" height="3" rx="1.5" fill="#000000" fill-opacity="0.16"/>
)svg").arg(win.x()).arg(win.y()).arg(win.width()).arg(win.height())
       .arg(win.x() + 12).arg(win.y() + 4).arg(win.width() - 24);

    const QString lit = "#2c5a2a";
    const qreal digitH = 86;
    const qreal top = win.y() + 14;
    auto layout = layoutText(u"42", u"°C", digitH);
    const qreal w = layoutWidth(layout);
    const qreal x0 = win.x() + (win.width() - w) / 2.0;

    auto drawGlyphs = [&](bool shadow) {
        for (const PlacedGlyph &g: layout) {
            const QRectF &c = g.cell;
            const qreal off = shadow ? 2.2 : 0.0;
            QTransform t(1, 0, -kSlant, 1, x0 + c.left() + kSlant * c.height() + off, top + c.top() + off);
            const SegmentMask drawn = shadow ? g.glyph.lit : (g.glyph.lit | g.glyph.ghost);
            for (SegmentMask bit = 1; bit <= kLastSegment; bit <<= 1) {
                if (!(drawn & bit)) continue;
                const QPolygonF poly = t.map(segmentPolygon(static_cast<Segment>(bit), c.size()));
                if (shadow) s << "<polygon fill=\"#000000\" fill-opacity=\"0.16\" points=\"" << pts(poly) << "\"/>\n";
                else if (g.glyph.lit & bit) s << "<polygon fill=\"" << lit << "\" points=\"" << pts(poly) << "\"/>\n";
                else s << "<polygon fill=\"" << lit << "\" fill-opacity=\"0.10\" points=\"" << pts(poly) << "\"/>\n";
            }
        }
    };
    drawGlyphs(true);
    drawGlyphs(false);

    // Bar graph along the bottom of the LCD window.
    const int n = 8, on = 5;
    const qreal bh = 13, by = win.bottom() - 13 - bh, bx = win.x() + 22, bw = win.width() - 44;
    const qreal gap = 5, sw = (bw - kSlant * bh - (n - 1) * gap) / n;
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i < n; ++i) {
            if (pass == 0 && i >= on) continue;
            const qreal x = bx + i * (sw + gap) + (pass == 0 ? 2.2 : 0), y = by + (pass == 0 ? 2.2 : 0);
            QPolygonF p{{{x + kSlant * bh, y}, {x + kSlant * bh + sw, y}, {x + sw, y + bh}, {x, y + bh}}};
            if (pass == 0) s << "<polygon fill=\"#000000\" fill-opacity=\"0.16\" points=\"" << pts(p) << "\"/>\n";
            else s << "<polygon fill=\"" << lit << "\"" << (i < on ? "" : " fill-opacity=\"0.10\"") << " points=\"" << pts(p) << "\"/>\n";
        }
    s << "</svg>\n";
    QFile f(out);
    if (!f.open(QIODevice::WriteOnly)) {
        qCritical("cannot write %s", qPrintable(out));
        return 1;
    }
    f.write(svg.toUtf8());
    return 0;
}
