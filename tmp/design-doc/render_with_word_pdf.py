"""Use packaged DOCX rasterizer with a PDF exported by Microsoft Word on Windows."""
import sys, importlib.util, shutil
from pathlib import Path
skill = Path(r'C:\Users\CI\.codex\plugins\cache\openai-primary-runtime\documents\26.905.11957\skills\documents')
spec = importlib.util.spec_from_file_location('packaged_render_docx', skill/'render_docx.py')
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
pdf = Path(sys.argv.pop(1)).resolve()
def convert(input_path, user_profile, convert_tmp_dir, stem, verbose=False):
    target = Path(convert_tmp_dir)/(stem+'.pdf')
    shutil.copy2(pdf, target)
    return str(target), 'PDF exported with Microsoft Word; bundled Poppler rasterization'
mod.convert_to_pdf = convert
mod.main()
