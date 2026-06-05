import os
for f in os.listdir('.'):
    if f.endswith('.txt') and not f.endswith('.utf8.txt'):
        try:
            with open(f, 'rb') as file:
                content = file.read()
            if content.startswith(b'\xff\xfe') or content.startswith(b'\xfe\xff') or b'\x00' in content:
                text = content.decode('utf-16')
                utf8_name = f.replace('.txt', '_utf8.txt')
                with open(utf8_name, 'w', encoding='utf-8') as out:
                    out.write(text)
                print(f"Converted {f} to {utf8_name}")
        except Exception as e:
            print(f"Error converting {f}: {e}")
