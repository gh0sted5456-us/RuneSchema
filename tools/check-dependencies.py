from pathlib import Path
import hashlib, json, sys

lock_path=Path(__file__).with_name('dependency-lock.json')
lock=json.loads(lock_path.read_text())
for entry in lock:
    root=Path(entry['path'])
    digest=hashlib.sha256()
    files=sorted(p for p in root.rglob('*') if p.is_file() and p.suffix.lower() in {'.h','.hpp','.inl','.cpp','.cc','.c'} and '.git' not in p.parts)
    for path in files:
        digest.update(path.relative_to(root).as_posix().encode()+b'\0')
        digest.update(hashlib.sha256(path.read_bytes()).digest())
    if len(files)!=entry['files'] or digest.hexdigest()!=entry['treeSha256']:
        sys.exit(f"Dependency source/header drift: {root}. Review changes and update the lock before rebuilding.")
print(f"Verified {len(lock)} dependency source/header trees")
