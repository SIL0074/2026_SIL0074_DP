import re

with open('web/index.html', 'r', encoding='utf-8') as f:
    content = f.read()

content = re.sub(r'\.node-spark\s*\{[^}]*\}', '', content)

content = re.sub(
    r'function getSparklineSVG\([^)]*\)\s*\{.*?return `<svg class="node-spark"[^>]*>.*?<\/svg>`;\s*\}',
    'function getSparklineSVG(id){return "";}',
    content,
    flags=re.DOTALL
)

content = re.sub(
    r'async function fetchAllSparklines\([^)]*\)\s*\{.*?catch\(e\)\{console\.error\(\'Sparklines error:\',e\);\}\s*\}',
    'async function fetchAllSparklines(){}',
    content,
    flags=re.DOTALL
)

content = re.sub(r'\$\{sparkSVG\}', '', content)

content = re.sub(r'const sparkSVG\s*=\s*[^;]+;', '', content)

with open('web/index.html', 'w', encoding='utf-8') as f:
    f.write(content)

print("Done")
