/// SVG timeline generation from CPL reel data.
use std::io::Write;

use crate::cpl::Cpl;

/// Generate an SVG timeline visualization of a CPL.
pub fn write_timeline_svg<W: Write>(cpl: &Cpl, writer: &mut W) -> std::io::Result<()> {
    let total_duration: i64 = cpl.reels.iter().map(|r| r.picture.duration).sum();
    if total_duration <= 0 {
        return Ok(());
    }

    let width = 800.0;
    let height = 120.0;
    let bar_height = 40.0;
    let bar_y = 50.0;

    writeln!(
        writer,
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"{width}\" height=\"{height}\" viewBox=\"0 0 {width} {height}\">"
    )?;
    writeln!(
        writer,
        "<text x=\"10\" y=\"20\" font-family=\"sans-serif\" font-size=\"14\" fill=\"#333\">{}</text>",
        cpl.content_title
    )?;
    writeln!(
        writer,
        "<text x=\"10\" y=\"38\" font-family=\"sans-serif\" font-size=\"11\" fill=\"#666\">Total: {total_duration} frames</text>"
    )?;

    let colors = [
        "#4a90d9", "#50c878", "#f5a623", "#d0021b", "#9b59b6", "#1abc9c",
    ];
    let mut x = 10.0;
    for (i, reel) in cpl.reels.iter().enumerate() {
        let reel_width = (reel.picture.duration as f64 / total_duration as f64) * (width - 20.0);
        let color = colors[i % colors.len()];
        writeln!(
            writer,
            "<rect x=\"{x}\" y=\"{bar_y}\" width=\"{reel_width}\" height=\"{bar_height}\" fill=\"{color}\" stroke=\"#fff\" stroke-width=\"1\"/>"
        )?;
        if reel_width > 30.0 {
            let cx = x + reel_width / 2.0;
            let cy = bar_y + bar_height / 2.0 + 4.0;
            writeln!(
                writer,
                "<text x=\"{cx}\" y=\"{cy}\" font-family=\"sans-serif\" font-size=\"10\" fill=\"#fff\" text-anchor=\"middle\">R{}</text>",
                i + 1
            )?;
        }
        x += reel_width;
    }

    writeln!(writer, "</svg>")?;
    Ok(())
}
