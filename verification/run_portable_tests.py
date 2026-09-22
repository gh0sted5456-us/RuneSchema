"""Optional author-side tests; never called by the user's Build RuneSchema.bat.
Needs GCC/Clang (C++23), Python, and a POSIX environment for file-I/O assertions.
Writes only to a temporary directory. Does not compile a UE4SS DLL.
"""
from pathlib import Path
import argparse,subprocess,tempfile,sys
p=argparse.ArgumentParser();p.add_argument('--compiler',default='g++');p.add_argument('--sanitize',action='store_true');args=p.parse_args()
root=Path(__file__).resolve().parents[1];here=root/'verification'
flags=['-std=c++23','-Wall','-Wextra','-Werror','-pedantic','-pthread','-I'+str(root/'raw/include')]
if args.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g']
with tempfile.TemporaryDirectory(prefix='runeschema-helpy-v10-') as temp:
    for name in ['F2ModelTests','F2BrowserTests','F2PreferenceFileTests','F2IndexTests','F2ProgressTests','F2CacheMigrationTests','HelpyV9Tests','HelpyV10Tests']:
        exe=Path(temp)/name
        subprocess.run([args.compiler,*flags,str(here/(name+'.cpp')),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],cwd=temp,check=True)
    subprocess.run([sys.executable,str(here/'check_identifiers.py')],cwd=temp,check=True)
for test in ['check_default_object_pointer.py','check_cooked_policy.py','check_provenance_identity.py','check_metadata_registry.py','check_helpy_npc.py']:
    command=[sys.executable,str(here/test),'--compiler',args.compiler]
    if args.sanitize:command+=['--sanitize']
    subprocess.run(command,check=True)
subprocess.run([sys.executable,str(here/'source_checks.py')],check=True)

subprocess.run([sys.executable,str(here/'check_settings_layout.py')],check=True)

subprocess.run([sys.executable,str(here/'helpy_source_checks.py')],check=True)

subprocess.run([sys.executable,str(here/"helpy_npc_source_checks.py")],check=True)

# This must parse the complete shipped schema header, not just UI/model code.
subprocess.run([sys.executable, str(here/"check_loader_schema_syntax.py"), "--compiler", args.compiler], check=True)
