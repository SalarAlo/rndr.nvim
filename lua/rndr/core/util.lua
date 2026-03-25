local M = {}

function M.plugin_root()
	local source = debug.getinfo(1, "S").source:sub(2)
	return vim.fn.fnamemodify(source, ":p:h:h:h:h")
end

function M.notify(message, level)
	vim.schedule(function()
		vim.notify(message, level or vim.log.levels.INFO, { title = "rndr.nvim" })
	end)
end

function M.is_valid_buffer(target_buf)
	return target_buf and vim.api.nvim_buf_is_valid(target_buf)
end

function M.is_valid_window(target_win)
	return target_win and vim.api.nvim_win_is_valid(target_win)
end

function M.extension_allowed(ext, allowed_extensions)
	for _, allowed_ext in ipairs(allowed_extensions or {}) do
		if ext == allowed_ext:lower() then
			return true
		end
	end

	return false
end

function M.file_extension(path)
	return vim.fn.fnamemodify(path, ":e"):lower()
end

function M.is_hex_color(value)
	return type(value) == "string" and value:match("^[0-9a-fA-F]+$") ~= nil
end

return M
