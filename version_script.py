Import("env")

with open("version.txt", "r") as f:
    version = f.read().strip()

env.Append(CPPDEFINES=[("VERSION", f'\\"v{version}\\"')])
