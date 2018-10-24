""" To use this you need to have the following plugins installed
"""   with your favorite plugin manager:
"""
"""     Plugin 'justmao945/vim-clang'
"""     Plugin 'w0rp/ale'
"""
""" And also have the following lines in your ~/.vimrc, so that
"""   the project specific .vimrc will get evaluated:
"""     
"""      set exrc
"""      set secure
"""
""" Then run the clang-complete-config target:
"""
"""     make docker-clang-complete-config
"""       (or without docker- if not using docker)
"""

let s:path = resolve(expand('<sfile>:p:h'))

let g:clang_dotfile = s:path . '/.clang_complete'
let g:clang_exec = s:path . '/scripts/vim-clang'

let g:ale_c_clangtidy_executable = s:path . '/scripts/vim-clang-tidy'
let g:ale_cpp_clangtidy_executable = s:path . '/scripts/vim-clang-tidy'

let g:ale_c_build_dir = s:path

set expandtab
set tabstop=2
set softtabstop=2
set shiftwidth=2
set autoindent

let $_VIM_PROJ_DIR = s:path

set path+=$_VIM_PROJ_DIR/package
set path+=$_VIM_PROJ_DIR/package/libpiksi/libpiksi/include

autocmd FileType c setlocal cindent
autocmd FileType cpp setlocal cindent

autocmd FileType c let b:ale_linters = { 'c': ['clangtidy'] }
autocmd FileType cpp let b:ale_linters = { 'cpp' : ['clangtidy'] }

let g:clang_tidy_checks = [
      \ 'bugprone-*', 'cert-*-c', 'performance-*', 'readability-*', 'misc-*',
      \ 'clange-analyzer-core.*', 'clang-analyzer-nullability.*', 'clang-analyzer-optin.*',
      \ 'clange-analyzer-unix.*', 'clang-analyzer-valist.*', 'clang-analyzer-security.*',
      \ '-readability-braces-around-statements',
      \ ]

autocmd FileType c let b:ale_c_clangtidy_checks = g:clang_tidy_checks
autocmd FileType cpp let b:ale_cpp_clangtidy_checks = g:clang_tidy_checks
