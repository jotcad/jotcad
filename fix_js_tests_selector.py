import os
import glob
import re

test_files = glob.glob('fs/test/*.js') + glob.glob('geo/test/*.js')

for file_path in test_files:
    with open(file_path, 'r') as f:
        content = f.read()

    # 1. Fix writeData(path, params, data) -> writeData({path, parameters: params}, data)
    # This is complex to regex safely, so we focus on common patterns in the tests.
    content = re.sub(r"writeData\(\s*'([^']+)',\s*({[^}]+}),\s*([^)]+)\)", 
                     r"writeData({ path: '\1', parameters: \2 }, \3)", content)
    
    # 2. Fix readData(path, params) -> readData({path, parameters: params})
    content = re.sub(r"readData\(\s*'([^']+)',\s*({[^}]+})\)", 
                     r"readData({ path: '\1', parameters: \2 })", content)

    # 3. Fix readText(path, params) -> readText({path, parameters: params})
    content = re.sub(r"readText\(\s*'([^']+)',\s*({[^}]+})\)", 
                     r"readText({ path: '\1', parameters: \2 })", content)

    # 4. Fix link(path, params, path, params)
    content = re.sub(r"link\(\s*'([^']+)',\s*({[^}]*}),\s*'([^']+)',\s*({[^}]*})\)",
                     r"link({ path: '\1', parameters: \2 }, { path: '\3', parameters: \4 })", content)

    with open(file_path, 'w') as f:
        f.write(content)

