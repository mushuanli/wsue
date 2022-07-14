local function setup_completions()
  local cmp = require('cmp')

  local kind_icons = {
    Class         = '',
    Color         = '',
    Constant      = '',
    Constructor   = '',
    Enum          = '',
    EnumMember    = '',
    Event         = '',
    Field         = '',
    File          = '',
    Folder        = '',
    Function      = '',
    Interface     = '',
    Keyword       = '',
    Method        = 'm',
    Module        = '',
    Operator      = '',
    Property      = '',
    Reference     = '',
    Snippet       = '',
    Struct        = '',
    Text          = '',
    TypeParameter = '',
    Unit          = '',
    Value         = '',
    Variable      = '',
  }

  cmp.setup {
    snippet = {
      expand = function(args)
        local luasnip = require('luasnip')
        luasnip.lsp_expand(args.body)
      end
    },
    mapping = {
      ['<c-b>'] = cmp.mapping(cmp.mapping.scroll_docs(-1), {'i', 'c'}),
      ['<c-f>'] = cmp.mapping(cmp.mapping.scroll_docs(1), {'i', 'c'}),
      ['<c-j>'] = cmp.mapping.select_next_item(),
      ['<c-k>'] = cmp.mapping.select_prev_item(),
      ['<c-space>'] = cmp.mapping(cmp.mapping.complete(), {'i', 'c'}),
      ['<cr>'] = cmp.mapping.confirm {select = true},
    },
    formatting = {
      fields = {'abbr', 'kind', 'menu'},
      format = function(entry, item)
        item.kind = string.format('[%s', kind_icons[item.kind], item.kind)
        item.menu = string.format('%s]', entry.source.name)
        return item
      end
    },
    sources = {
      {name = 'nvim_lsp'},
      {name = 'nvim_lua'},
      {name = 'path'},
      {name = 'buffer'}
    },
    confirm_opts = {
      behavior = cmp.ConfirmBehavior.Replace,
      select = false,
    }
  }
end

local function setup_lsp_keymaps(bufnr)
  local opts = {noremap = true, silent = true}
  local keymap = vim.api.nvim_buf_set_keymap

  local normal = 'n'

  keymap(bufnr, normal, '<leader>ca', '<cmd>lua vim.lsp.buf.code_action()<cr>', opts)
  keymap(bufnr, normal, '<leader>rn', '<cmd>lua vim.lsp.buf.rename()<cr>', opts)
  keymap(bufnr, normal, '[d', '<cmd>lua vim.diagnostic.goto_prev()<cr>', opts)
  keymap(bufnr, normal, ']d', '<cmd>lua vim.diagnostic.goto_next()<cr>', opts)
  keymap(bufnr, normal, 'gl', '<cmd>lua vim.lsp.diagnostic.show_line_diagnostics()<cr>', opts)
  keymap(bufnr, normal, 'gc', '<cmd>lua vim.lsp.buf.incoming_calls()<cr>', opts)

  -- c-x c-o
  vim.api.nvim_buf_set_option(bufnr, 'omnifunc', 'v:lua.vim.lsp.omnifunc')

  -- Mappings.
  -- See `:help vim.lsp.*` for documentation on any of the below functions
  keymap(bufnr, 'n', 'gD', '<cmd>lua vim.lsp.buf.declaration()<CR>', opts)
  keymap(bufnr, 'n', 'gd', '<cmd>lua vim.lsp.buf.definition()<CR>', opts)
  keymap(bufnr, 'n', 'K', '<cmd>lua vim.lsp.buf.hover()<CR>', opts)
  keymap(bufnr, 'n', 'gi', '<cmd>lua vim.lsp.buf.implementation()<CR>', opts)
  keymap(bufnr, 'n', '<C-k>', '<cmd>lua vim.lsp.buf.signature_help()<CR>', opts)
  keymap(bufnr, 'n', '<space>wa', '<cmd>lua vim.lsp.buf.add_workspace_folder()<CR>', opts)
  keymap(bufnr, 'n', '<space>wr', '<cmd>lua vim.lsp.buf.remove_workspace_folder()<CR>', opts)
  keymap(bufnr, 'n', '<space>wl', '<cmd>lua print(vim.inspect(vim.lsp.buf.list_workspace_folders()))<CR>', opts)
  keymap(bufnr, 'n', '<space>D', '<cmd>lua vim.lsp.buf.type_definition()<CR>', opts)
  keymap(bufnr, 'n', '<space>rn', '<cmd>lua vim.lsp.buf.rename()<CR>', opts)
  keymap(bufnr, 'n', '<space>ca', '<cmd>lua vim.lsp.buf.code_action()<CR>', opts)
  keymap(bufnr, 'n', 'gr', '<cmd>lua vim.lsp.buf.references()<CR>', opts)
  keymap(bufnr, 'n', '<space>f', '<cmd>lua vim.lsp.buf.formatting()<CR>', opts)
end

local function setup_lsp_settings()
  local lsp_installer = require('nvim-lsp-installer')

  lsp_installer.on_server_ready(function(server)
    local cmp_nvim_lsp = require('cmp_nvim_lsp')
    local capabilities = vim.lsp.protocol.make_client_capabilities()

    local opts = {
      on_attach = function(_, bufnr) setup_lsp_keymaps(bufnr) end,
      capabilities = cmp_nvim_lsp.update_capabilities(capabilities)
    }

    if server.name == 'sumneko_lua' then
      opts.settings = {
        Lua = {
          diagnostics = {
            globals = {'vim'}
          }
        }
      }
    end

    server:setup(opts)
  end)
end

local function setup_statusbar()
  require("indent_blankline").setup {
    -- for example, context is off by default, use this to turn it on
    show_current_context = true,
    show_current_context_start = true,
  }

  require('lualine').setup {
    options = {
      icons_enabled = true,
      theme = 'auto', -- based on current vim colorscheme
      -- not a big fan of fancy triangle separators
      component_separators = { left = '', right = '' },
      section_separators = { left = '', right = '' },
      disabled_filetypes = {},
      always_divide_middle = true,
    },
    sections = {
      -- left
      lualine_a = { 'mode' },
      lualine_b = { 'branch', 'diff', 'diagnostics' },
      lualine_c = { 'filename' },
      -- right
      lualine_x = { 'encoding', 'fileformat', 'filetype' },
      lualine_y = { 'progress' },
      lualine_z = { 'location' }
    },
    inactive_sections = {
      lualine_a = { 'filename' },
      lualine_b = {},
      lualine_c = {},
      lualine_x = { 'location' },
      lualine_y = {},
      lualine_z = {}
    },
    tabline = {},
    extensions = {}
  }
end

local function install_lsp_servers()
  local lsp_installer_servers = require('nvim-lsp-installer.servers')

  local servers = {
    'bashls',
    'clangd',
    'rust_analyzer',
    'sumneko_lua',
  }

  for _, server_name in pairs(servers) do
    local available, server = lsp_installer_servers.get_server(server_name)
    if available and not server:is_installed() then
      server:install()
    end
  end
end

setup_completions()
setup_statusbar()
setup_lsp_settings()
install_lsp_servers()
