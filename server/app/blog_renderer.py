import markdown
from html import escape


class BlogRenderer:
    """Renders Markdown to HTML with safe HTML escaping."""

    @staticmethod
    def render_markdown(markdown_body: str) -> str:
        """Convert Markdown text to HTML."""
        try:
            html = markdown.markdown(
                markdown_body,
                extensions=["extra", "codehilite", "toc"],
            )
            return html
        except Exception as e:
            # Fallback: return escaped text if rendering fails
            return f"<p>{escape(markdown_body)}</p>"

    @staticmethod
    def render_post_html(title: str, markdown_body: str, categories: list) -> str:
        """Render a complete blog post HTML page."""
        content_html = BlogRenderer.render_markdown(markdown_body)
        category_html = ""
        if categories:
            cat_items = "".join(f"<span class='category'>{escape(cat)}</span>" for cat in categories)
            category_html = f"<div class='categories'>{cat_items}</div>"

        html = f"""<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{escape(title)}</title>
    <style>
        body {{ font-family: sans-serif; line-height: 1.6; max-width: 800px; margin: 0 auto; padding: 20px; }}
        h1 {{ color: #333; border-bottom: 2px solid #007acc; padding-bottom: 10px; }}
        .categories {{ margin: 10px 0; }}
        .category {{ display: inline-block; background-color: #007acc; color: white; padding: 5px 10px; margin-right: 5px; border-radius: 3px; font-size: 0.9em; }}
        pre {{ background-color: #f5f5f5; padding: 10px; overflow-x: auto; border-radius: 3px; }}
        code {{ background-color: #f5f5f5; padding: 2px 6px; border-radius: 3px; font-family: monospace; }}
        a {{ color: #007acc; text-decoration: none; }}
        a:hover {{ text-decoration: underline; }}
    </style>
</head>
<body>
    <h1>{escape(title)}</h1>
    {category_html}
    <div class='content'>
        {content_html}
    </div>
</body>
</html>"""
        return html
