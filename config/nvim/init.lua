-- ref: https://github.com/fsareshwala/dotfiles/blob/master/.config/nvim/init.lua
local function init_ui()
  local isWindows = vim.loop.os_uname().sysname == "Windows_NT"
  -- utf8
  vim.g.encoding                 = "UTF-8"
  vim.opt.fileencoding           = "utf-8"
  vim.g.homedir                  = os.getenv('HOME')
  vim.opt.clipboard              = "unnamedplus"

  -- improve load speed
  vim.g.do_filetype_lua          = 1
  vim.g.did_load_filetypes       = 0
  vim.g.loaded_gzip              = 1
  vim.g.loaded_tar               = 1
  vim.g.loaded_tarPlugin         = 1
  vim.g.loaded_zip               = 1
  vim.g.loaded_zipPlugin         = 1
  vim.g.loaded_getscript         = 1
  vim.g.loaded_getscriptPlugin   = 1
  vim.g.loaded_vimball           = 1
  vim.g.loaded_vimballPlugin     = 1
  vim.g.loaded_matchit           = 1
  vim.g.loaded_matchparen        = 1
  vim.g.loaded_2html_plugin      = 1
  vim.g.loaded_logiPat           = 1
  vim.g.loaded_rrhelper          = 1
  vim.g.loaded_netrw             = 1
  vim.g.loaded_netrwPlugin       = 1
  vim.g.loaded_netrwSettings     = 1
  vim.g.loaded_netrwFileHandlers = 1
  vim.g.loaded_remote_plugins    = 1
  vim.g.loaded_shada_plugin      = 1


  --  UI
  vim.opt.background             = 'dark'
  vim.opt.showcmd                = true
  vim.opt.ttyfast                = true
  vim.opt.lazyredraw             = true
  vim.opt.termguicolors          = true   -- 样式

  vim.opt.updatetime             = 300    -- smaller updatetime
  -- 设置 timeoutlen 为等待键盘快捷键连击时间500毫秒，可根据需要设置
  -- 遇到问题详见：https://github.com/nshen/learn-neovim-lua/issues/1
  vim.opt.timeoutlen             = 500
  vim.opt.shada                  = "!,'1000,<50,s10,h"

  vim.opt.background             = 'dark'
  if isWindows then
    vim.cmd [[
      language en_US
      source $VIMRUNTIME/mswin.vim
      call rpcnotify(0, 'Gui', 'Option', 'Popupmenu', 0)
      ]]
  end

  vim.cmd [[
  behave xterm
  colorscheme gruvbox
  ]]

  return isWindows
end

local function set_options()

  -- Edit
  -- 当文件被外部程序修改时，自动加载
  vim.opt.autoread               = true
  vim.opt.autowriteall           = true
  vim.opt.confirm                = true

  -- 鼠标支持
  vim.opt.mouse                  = 'nvi'  -- 鼠标模式
  vim.opt.mousehide              = true
  vim.opt.number                 = true   -- 显示行号
  vim.opt.ruler                  = true   -- 显示
  vim.opt.splitbelow             = true   -- split window 从下边和右边出现
  vim.opt.splitright             = true
  vim.opt.cursorline             = false   -- 高亮所在行,比较费性能
  vim.wo.signcolumn              = "yes"  -- 显示左侧图标指示列
  vim.wo.colorcolumn             = "80"   -- 右侧参考线，超过表示代码太长了，考虑换行
  vim.opt.cmdheight              = 1      -- 命令行高为2，提供足够的显示空间
  vim.wo.wrap                    = false  -- 禁止折行
  vim.opt.showtabline            = 2      -- 永远显示 tabline
  vim.opt.laststatus             = 3      -- enable global status line
  vim.opt.showmode               = false  -- 使用增强状态栏插件后不再需要 vim 的模式提示
  vim.opt.backspace              = {'eol', 'start', 'indent'}
  vim.opt.spell                  = false  -- 禁止拼写检查
  vim.opt.showmatch              = true   -- 显示括号匹配, 时间3秒
  vim.opt.matchtime              = 3

  -- jkhl 移动时光标周围保留8行
  vim.opt.scrolloff              = 8
  vim.opt.sidescrolloff          = 15
  vim.opt.scrolljump             = 3
  vim.opt.whichwrap              = "<,>,[,]"  -- 光标在行首尾时<Left><Right>可以跳到下一行
  vim.opt.hidden                 = true   -- 允许隐藏被修改过的buffer

  vim.opt.hlsearch               = true   -- 搜索不要高亮
  vim.opt.incsearch              = true   -- 边输入边搜索
  vim.opt.ignorecase             = true   -- 搜索大小写不敏感，除非包含大写
  vim.opt.smartcase              = true
  vim.opt.grepprg                = "rg --with-filename --no-heading --line-number --column --hidden --smart-case --follow"
  vim.opt.grepformat             = "%f:%l:%c:%m"

  vim.opt.formatoptions          = 'cjlnqrt'  -- 代码缩进
  vim.opt.autoindent             = true
  vim.opt.smartindent            = true
  vim.opt.copyindent             = true
  vim.opt.preserveindent         = true
  vim.opt.tabstop                = 8      -- tab缩进
  vim.opt.softtabstop            = 2
  vim.opt.expandtab              = true
  vim.opt.smarttab               = true
  -- >> << 时移动长度
  vim.opt.shiftwidth             = 2

  -- 禁止创建备份文件
  vim.opt.backup                 = false
  vim.opt.writebackup            = false
  vim.opt.swapfile               = false
  vim.opt.undofile               = true
  vim.opt.sessionoptions         = "buffers,curdir,folds,help,tabpages,winsize,winpos,terminal"
  vim.opt.diffopt                = "internal,filler,closeoff,vertical,foldcolumn:0"

  vim.g.completeopt              = "menu,menuone,noselect,noinsert"   -- 自动补全不自动选中
  vim.opt.pumheight              = 10     -- 补全最多显示10行
  vim.opt.list                   = false  -- 是否显示不可见字符
  vim.opt.listchars              = {tab = '|-', trail = '-', extends = '>', precedes = '<',eol = '↴'} -- 不可见字符的显示，这里只把空格显示为一个点
  -- vim.opt.shortmess        = vim.opt.shortmess .. "c"     -- Dont' pass messages to |ins-completin menu|
  vim.opt.wildmode               = 'list:longest,full'
  vim.opt.wildignore:append({'**/node_modules/**','.git','*.so', '*.o','*.dll','*.exe','*.obj','*.d','*.zip','*.e','*.pyc','*.pyo','*~','*.bak','*.sw?','*.jpg','*.bmp','*.gif','*.png'})
  vim.opt.wildmenu               = true

  -- status bar
  local statusLineComponents = {
      -- Used to put the mode, but if terminal can change cursor shape, it really isn't required.
      '%2*%-3.3n%0* %f ',   -- File name
      "%h%1*%m%r%w%0* ",
      "%{&ff!='unix'?'['.&ff.'] ':''}",  -- File format (unix vs. dos)
      "%{(&fenc!='utf-8'&&&fenc!='')?'['.&fenc.'] ':''}",
      '%{&encoding}', -- current encoding
      '%y %m%r',   -- file type, modify, readonly
      '%=',    -- makes following right aligned
      "%#warningmsg#",
      "%*",
      "0x%-8B %-14.(%l,%c%V%) %<%P ",
      'C:%c ',  -- column number
      '%p%% ',  -- percentage through file
    '%{strftime("%d/%m - %H:%M")}'
  }

  vim.opt.statusline = table.concat(statusLineComponents)
end

local function set_keymaps()
  local silent = { silent = true, noremap = true }
  --[[
  local function nmap(lhs, rhs) vim.api.nvim_set_keymap("", lhs, rhs, silent) end
  local function nnmap(lhs, rhs) vim.api.nvim_set_keymap("n", lhs, rhs, silent) end
  local function inmap(lhs, rhs) vim.api.nvim_set_keymap("i", lhs, rhs, silent) end
  local function cnmap(lhs, rhs) vim.api.nvim_set_keymap("c", lhs, rhs, silent) end
  local function vnmap(lhs, rhs) vim.api.nvim_set_keymap("v", lhs, rhs, silent) end
  local function xnmap(lhs, rhs) vim.api.nvim_set_keymap("x", lhs, rhs, silent) end
  ]]--

  -- vim.api.nvim_set_keymap("n",'<F3>',':NvimTreeToggle<cr>',silent)
  vim.api.nvim_set_keymap("n",'<F4>','zO',silent)
  vim.api.nvim_set_keymap("n",'<F5>','zR',silent)
  vim.api.nvim_set_keymap("n",'<F6>','zM',silent)
  vim.api.nvim_set_keymap("n",'<F7>',':cn<CR>',silent)
  -- vim.api.nvim_set_keymap("v","<LeftRelease>", '"*ygv',silent)
  vim.api.nvim_set_keymap("n",'<c-h>', '<c-w>h',silent)
  vim.api.nvim_set_keymap("n",'<c-j>', '<c-w>j',silent)
  vim.api.nvim_set_keymap("n",'<c-k>', '<c-w>k',silent)
  vim.api.nvim_set_keymap("n",'<c-l>', '<c-w>l',silent)
  vim.api.nvim_set_keymap("n",'<leader>s', ':vsplit<cr>',silent)
  vim.api.nvim_set_keymap("n",'<leader>t', ':tabnew<cr>',silent)
  vim.api.nvim_set_keymap("n",'H', ':tabprev<cr>',silent)
  vim.api.nvim_set_keymap("n",'L', ':tabnext<cr>',silent)
  vim.api.nvim_set_keymap("n",'<leader>q', ':bd<cr>',silent)
  -- resize with arrows
  vim.api.nvim_set_keymap("n",'<M-up>', ':resize +2<cr>',silent)
  vim.api.nvim_set_keymap("n",'<M-down>', ':resize -2<cr>',silent)
  vim.api.nvim_set_keymap("n",'<M-left>', ':vertical resize -2<cr>',silent)
  vim.api.nvim_set_keymap("n",'<M-right>', ':vertical resize +2<cr>',silent)
  -- format
  vim.api.nvim_set_keymap("v",'<', '<gv',silent)
  vim.api.nvim_set_keymap("v",'>', '>gv',silent)
  -- stay at current word when using star search
  vim.api.nvim_set_keymap("n",'ga', '<Plug>(EasyAlign)',silent)
  vim.api.nvim_set_keymap("x",'ga', '<Plug>(EasyAlign)',silent)

  -- telescope keymaps
  -- vim.api.nvim_set_keymap("n",'<leader>ff', '<cmd>Telescope find_files<cr>',silent)
  -- vim.api.nvim_set_keymap("n",'<leader>fg', '<cmd>Telescope live_grep<cr>',silent)
  -- vim.api.nvim_set_keymap("n",'<leader>fb', '<cmd>Telescope buffers<cr>',silent)
  -- vim.api.nvim_set_keymap("n",'<leader>fh', '<cmd>Telescope help_tags<cr>',silent)
  -- vim.api.nvim_set_keymap("n",'<leader>fG', '<cmd>Telescope grep_string<cr>',silent)
  -- vim.api.nvim_set_keymap("n",'<leader>z', '<cmd>Telescope spell_suggest<cr>',silent)
  -- vim.api.nvim_set_keymap("n",'<leader>fx', '<cmd>Telescope lsp_dynamic_workspace_symbols<cr>',silent)
  vim.api.nvim_set_keymap("n","<leader>ff", [[<cmd>lua require('telescope.builtin').find_files()<cr>]],silent)
  vim.api.nvim_set_keymap("n","<leader>fg", [[<cmd>lua require('telescope.builtin').live_grep()<cr>]],silent)
  vim.api.nvim_set_keymap("n","<leader>fb", [[<cmd>lua require('telescope.builtin').buffers()<cr>]],silent)
  vim.api.nvim_set_keymap("n","<leader>fh", [[<cmd>lua require('telescope.builtin').help_tags()<cr>]],silent)
  -- vim.api.nvim_set_keymap("n","<leader>sf", [[<cmd>lua require('telescope.builtin').file_browser()<cr>]],silent)
  vim.api.nvim_set_keymap("n","<leader>/", [[<cmd>lua require'telescope.builtin'.current_buffer_fuzzy_find{}<CR>]],silent)
  vim.api.nvim_set_keymap("n","<C-p>", [[<cmd>lua require('telescope.builtin').find_files()<cr>]],silent)
  vim.api.nvim_set_keymap("n","<C-b>", [[<cmd>lua require('telescope.builtin').buffers()<cr>]],silent)
  vim.api.nvim_set_keymap("n","<C-a>", [[<cmd>lua require('telescope.builtin').live_grep()<cr>]],silent)
  vim.api.nvim_set_keymap("n","<C-s>", [[<cmd>lua require'telescope.builtin'.current_buffer_fuzzy_find{}<CR>]],silent)

  -- file tree keymaps
  vim.api.nvim_set_keymap("n",'<leader>n', '<cmd>NvimTreeToggle<cr>',silent)

  vim.api.nvim_set_keymap("n", "<leader>l", ":bnext<CR>", {noremap = true, silent = true})
  vim.api.nvim_set_keymap("n", "<leader>h", ":bprevious<CR>", {noremap = true, silent = true})
  vim.api.nvim_set_keymap("n", "<leader>e", ":NvimTreeToggle<CR>", {noremap = true, silent = true})
  vim.api.nvim_set_keymap("n", "<leader>do", ":DiffviewOpen<CR>", {noremap = true, silent = true})
  vim.api.nvim_set_keymap("n", "<leader>dc", ":DiffviewClose<CR>", {noremap = true, silent = true})
  vim.api.nvim_set_keymap("n", "gb", "<Cmd>BufferLinePick<CR>", {noremap = true, silent = true})
  vim.api.nvim_set_keymap("n", "[b", ":BufferLineCycleNext<CR>", {noremap = true, silent = true})
  vim.api.nvim_set_keymap("n", "]b", ":BufferLineCyclePrev<CR>", {noremap = true, silent = true})
  vim.api.nvim_set_keymap("n", "<leader>1", "<Cmd>BufferLineGoToBuffer 1<CR>", {noremap = true, silent = true})
  vim.api.nvim_set_keymap("n", "<leader>2", "<Cmd>BufferLineGoToBuffer 2<CR>", {noremap = true, silent = true})
  vim.api.nvim_set_keymap("n", "<leader>3", "<Cmd>BufferLineGoToBuffer 3<CR>", {noremap = true, silent = true})
  vim.api.nvim_set_keymap("n", "<leader>4", "<Cmd>BufferLineGoToBuffer 4<CR>", {noremap = true, silent = true})
  vim.api.nvim_set_keymap("n", "<leader>5", "<Cmd>BufferLineGoToBuffer 5<CR>", {noremap = true, silent = true})
  vim.api.nvim_set_keymap("n", "<leader>6", "<Cmd>BufferLineGoToBuffer 6<CR>", {noremap = true, silent = true})
  vim.api.nvim_set_keymap("n", "<leader>7", "<Cmd>BufferLineGoToBuffer 7<CR>", {noremap = true, silent = true})
  vim.api.nvim_set_keymap("n", "<leader>8", "<Cmd>BufferLineGoToBuffer 8<CR>", {noremap = true, silent = true})
  vim.api.nvim_set_keymap("n", "<leader>9", "<Cmd>BufferLineGoToBuffer 9<CR>", {noremap = true, silent = true})
end

local function install_plugins(isWindows)
  local data_path = vim.fn.stdpath('data')
  local packer_subfile = '/site/pack/packer/start/packer.nvim'
  local install_path = data_path .. packer_subfile
  local packer_repo = 'https://github.com/wbthomason/packer.nvim'

  if vim.fn.empty(vim.fn.glob(install_path)) then
    vim.fn.system({'git', 'clone', packer_repo, install_path})
    vim.cmd('packadd packer.nvim')
  end

  local packer = require('packer')

  packer.startup(function(use)
    use 'lewis6991/impatient.nvim'
    use 'wbthomason/packer.nvim'    -- let packer manage itself
    use "nathom/filetype.nvim"

    use 'ellisonleao/gruvbox.nvim'    -- let packer manage itself
    use 'junegunn/vim-easy-align'
    -- use 'chriskempson/base16-vim' -- colorscheme
    -- use 'jiangmiao/auto-pairs'    -- automatically insert/delete parenthesis, brackets, quotes
    -- use 'ojroques/vim-oscyank'    -- osc52 location independent clipboard
    -- use 'tpope/vim-abolish'       -- {} syntax (:Abolish, :Subvert), case style change (crc)
    -- use 'tpope/vim-commentary'    -- motions to comment lines out
    -- use 'tpope/vim-fugitive'      -- git bidings
    -- use 'tpope/vim-repeat'        -- allow plugins to override .
    -- use 'tpope/vim-surround'      -- motions to surround text with other text
    -- use 'rust-lang/rust.vim'      -- rust vim integration

    -- fuzzy finder over files, commands, lists, etc
    use {
      'nvim-telescope/telescope.nvim',
      requires = {
        'nvim-lua/plenary.nvim',
        {'nvim-telescope/telescope-fzf-native.nvim', run = 'make' },
      }
    }

    -- Better syntax highlighting
    use {
      "nvim-treesitter/nvim-treesitter",
      run = ":TSUpdate",
    }
    --

    -- file tree
    use {
      'kyazdani42/nvim-tree.lua',
      requires = {'kyazdani42/nvim-web-devicons'}
    }


    -- bufferline 显示标签页,与lualine配合使用
    use {'akinsho/bufferline.nvim', tag = "v2.*", requires = 'kyazdani42/nvim-web-devicons'}

    -- completion engine
    if not isWindows then
      use {
        'hrsh7th/nvim-cmp',
        requires = {
          'hrsh7th/cmp-buffer',       -- text in current buffer
          'hrsh7th/cmp-nvim-lua',     -- neovim lua api
          'hrsh7th/cmp-path',         -- filesystem paths
          'L3MON4D3/LuaSnip',         -- nvim-cmp requires a snippet engine for expansion
          'saadparwaiz1/cmp_luasnip'  -- completion source for luasnip
        }
      }

      -- language server protocol
      use {
        'neovim/nvim-lspconfig',
        requires = {
          'hrsh7th/cmp-nvim-lsp',    -- lsp based completions
          'williamboman/nvim-lsp-installer'
        }
      }
      --状态栏插件
      use {
        "nvim-lualine/lualine.nvim",
        requires = {"kyazdani42/nvim-web-devicons", opt = true}
      }
      use "lukas-reineke/indent-blankline.nvim"
    end
    --
  end)


end

local function setup_plugins()

  require('impatient')  -- .enable_profile()
  -- In init.lua or filetype.nvim's config file
  require("filetype").setup({
      overrides = {
          extensions = {
              -- Set the filetype of *.pn files to potion
              pn = "potion",
          },
          literal = {
              -- Set the filetype of files named "MyBackupFile" to lua
              MyBackupFile = "lua",
          },
          complex = {
              -- Set the filetype of any full filename matching the regex to gitconfig
              [".*git/config"] = "gitconfig", -- Included in the plugin
          },

          -- The same as the ones above except the keys map to functions
          function_extensions = {
              ["cpp"] = function()
                  vim.bo.filetype = "cpp"
                  -- Remove annoying indent jumping
                  vim.bo.cinoptions = vim.bo.cinoptions .. "L0"
              end,
              ["pdf"] = function()
                  vim.bo.filetype = "pdf"
                  -- Open in PDF viewer (Skim.app) automatically
                  vim.fn.jobstart(
                  "open -a skim " .. '"' .. vim.fn.expand("%") .. '"'
                  )
              end,
          },
          function_literal = {
              Brewfile = function()
                  vim.cmd("syntax off")
              end,
          },
          function_complex = {
              ["*.math_notes/%w+"] = function()
                  vim.cmd("iabbrev $ $$")
              end,
          },

          shebang = {
              -- Set the filetype of files with a dash shebang to sh
              dash = "sh",
          },
      },
  })

  require('bufferline').setup {
    options = {
      mode = "buffers", -- set to "tabs" to only show tabpages instead
      numbers = "none",
      close_command = "bdelete! %d", -- can be a string | function, see "Mouse actions"
      right_mouse_command = "bdelete! %d", -- can be a string | function, see "Mouse actions"
      left_mouse_command = "buffer %d", -- can be a string | function, see "Mouse actions"
      middle_mouse_command = nil, -- can be a string | function, see "Mouse actions"
      -- NOTE: this plugin is designed with this icon in mind,
      -- and so changing this is NOT recommended, this is intended
      -- as an escape hatch for people who cannot bear it for whatever reason
      indicator_icon = '▎',
      buffer_close_icon = '',
      modified_icon = '●',
      close_icon = '',
      left_trunc_marker = '|',
      right_trunc_marker = '|',
      --- name_formatter can be used to change the buffer's label in the bufferline.
      --- Please note some names can/will break the
      --- bufferline so use this at your discretion knowing that it has
      --- some limitations that will *NOT* be fixed.
      name_formatter = function(buf) -- buf contains a "name", "path" and "bufnr"
        -- remove extension from markdown files for example
        if buf.name:match('%.md') then
          return vim.fn.fnamemodify(buf.name, ':t:r')
        end
      end,
      max_name_length = 18,
      max_prefix_length = 15, -- prefix used when a buffer is de-duplicated
      tab_size = 18,
      diagnostics = "nvim_lsp",
      diagnostics_update_in_insert = false,
      diagnostics_indicator = function(count, level, diagnostics_dict, context)
        local s = " "
        for e, n in pairs(diagnostics_dict) do
          local sym = e == "error" and " "
          or (e == "warning" and " " or "")
          s = s .. sym
        end
        return s
      end,
      -- NOTE: this will be called a lot so don't do any heavy processing here
      custom_filter = function(buf_number, buf_numbers)
        -- filter out filetypes you don't want to see
        if vim.bo[buf_number].filetype ~= "<i-dont-want-to-see-this>" then
          return true
        end
        -- filter out by buffer name
        if vim.fn.bufname(buf_number) ~= "<buffer-name-I-dont-want>" then
          return true
        end
        -- filter out based on arbitrary rules
        -- e.g. filter out vim wiki buffer from tabline in your work repo
        if vim.fn.getcwd() == "<work-repo>" and vim.bo[buf_number].filetype ~= "wiki" then
          return true
        end
        -- filter out by it's index number in list (don't show first buffer)
        if buf_numbers[1] ~= buf_number then
          return true
        end
      end,
      offsets = { { filetype = "NvimTree", text = "File Explorer", text_align = "center" },
        { filetype = "SymbolsOutline", text = "Symbols Outline", text_align = "center" } },
      color_icons = true,
      show_buffer_icons = true, -- disable filetype icons for buffers
      show_buffer_close_icons = true,
      show_buffer_default_icon = true, -- whether or not an unrecognised filetype should show a default icon
      show_close_icon = true,
      show_tab_indicators = true,
      persist_buffer_sort = true, -- whether or not custom sorted buffers should persist
      -- can also be a table containing 2 custom separators
      -- [focused and unfocused]. eg: { '|', '|' }
      separator_style = "thin",
      enforce_regular_tabs = false,
      always_show_bufferline = true,
      sort_by = 'id'
    }
  }

end


local function setup_telescope()
  local telescope = require('telescope')
  local actions = require('telescope.actions')

  telescope.setup {
    defaults = {
      mappings = {
        i = {
          ['<C-p>'] = actions.preview_scrolling_up,
          ['<C-n>'] = actions.preview_scrolling_down,
          ['<C-j>'] = actions.move_selection_next,
          ['<C-k>'] = actions.move_selection_previous,
          ["<C-h>"] = "which_key",
            ['<c-d>'] = actions.delete_buffer
        }, -- n
        n = {
          ['<c-d>'] = actions.delete_buffer
        } -- i
      }
    },
    file_ignore_patterns = {"./node_modules"}
  }

  telescope.load_extension('fzf')
end

local function setup_treesitter()
  local configs = require('nvim-treesitter.configs')

  configs.setup {
    ensure_installed      = {"go", "lua", "javascript", "c"},
    sync_install          = false,
    highlight = {
      enable              = true,
      additional_vim_regex_highlighting = true,
    },
    -- 启用增量选择
    incremental_selection = {
      enable              = true,
      keymaps = {
        init_selection    = '<CR>',
        node_incremental  = '<CR>',
        node_decremental  = '<BS>',
        scope_incremental = '<TAB>',
      }
    },
    -- 启用基于Treesitter的代码格式化(=) . NOTE: This is an experimental feature.
    indent = {
      enable              = true
    }
  }

  -- 开启 Folding
  vim.wo.foldmethod = 'expr'
  vim.wo.foldexpr = 'nvim_treesitter#foldexpr()'
  -- 默认不要折叠
  -- https://stackoverflow.com/questions/8316139/how-to-set-the-default-to-unfolded-when-you-open-a-file
  vim.wo.foldlevel = 99
  -- vim.wo.foldopen -= 'search'
end

local function setup_filetree()
  local filetree = require('nvim-tree')

  filetree.setup({
    open_on_setup                      = true,
    hijack_cursor                      = true,
    hijack_unnamed_buffer_when_opening = false,
    open_on_tab                        = true,
    reload_on_bufenter                 = true,
    update_cwd                         = true,
    view    = {
      side                             = 'left',
      width                            = 35,
      number                           = false,
      relativenumber                   = false,
      signcolumn                       = "yes"
    },
    git     = {
      enable                           = false,
      ignore                           = false
    },
    filters = {
      dotfiles                         = true,
      custom                           = {'.git'},
    },
    update_focused_file = {
      enable                           = true,
      update_cwd                       = false
    },
    renderer            = {
      group_empty                      = true,
      full_name                        = true,
    },
    actions             = {
      open_file                        = {
        resize_window                  = true,
      },
    }
  })
end

local function setup_autocmds(isWindows)

  -- return to the same line when you reopen a file
  vim.cmd [[
    augroup line_return
    autocmd!
    autocmd BufReadPost *
        \ if line("'\"") > 0 && line("'\"") <= line('$') |
        \     execute 'normal! g`"zvzz' |
        \ endif
    augroup end
  ]]

  -- automatically delete all trailing whitespace and newlines at end of file on save
  vim.cmd [[
    augroup trailing_whitespace
    autocmd!
    autocmd BufWritePre * %s/\s\+$//e
    autocmd BufWritepre * %s/\n\+\%$//e
    augroup end
  ]]

  -- autoclose nvim-tree if it's the last buffer open
  vim.cmd [[
    augroup autoclose_nvim_tree
    autocmd!
    autocmd BufEnter * ++nested
      \ if winnr('$') == 1 && bufname() == 'NvimTree_' . tabpagenr() |
      \   quit |
      \ endif
    augroup end
  ]]
end


local function main()
  local isWindows = init_ui()
  set_options()
  set_keymaps()
  install_plugins(isWindows)
  setup_plugins()

  if not isWindows then
    require('exinit')
  end
  setup_telescope()
  setup_treesitter()
  setup_filetree()


  setup_autocmds(isWindows)
end

main()
