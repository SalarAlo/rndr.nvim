local util = require("rndr.core.util")

local M = {}

local function cache_dir()
	local dir = vim.fn.stdpath("cache") .. "/rndr"
	if vim.fn.isdirectory(dir) == 0 then
		vim.fn.mkdir(dir, "p")
	end

	return dir
end

local function cache_key_for(path)
	local uv = vim.uv or vim.loop
	local stat = uv and uv.fs_stat(path) or nil
	local mtime = stat and stat.mtime and (stat.mtime.sec or stat.mtime) or 0
	local size = stat and stat.size or 0
	return vim.fn.sha256(table.concat({ path, tostring(mtime), tostring(size) }, "\n"))
end

local function run_system_command(argv)
	vim.fn.system(argv)
	return vim.v.shell_error == 0
end

function M.available_svg_rasterizers()
	local tools = { "rsvg-convert", "magick", "convert" }
	local available = {}

	for _, tool in ipairs(tools) do
		if vim.fn.executable(tool) == 1 then
			table.insert(available, tool)
		end
	end

	return available
end

function M.rasterize_svg(path)
	local output_path = cache_dir() .. "/" .. cache_key_for(path) .. ".png"
	if vim.fn.filereadable(output_path) == 1 then
		return output_path
	end

	local commands = {
		{ "rsvg-convert", path, "-o", output_path },
		{ "magick", path, output_path },
		{ "convert", path, output_path },
	}

	for _, command in ipairs(commands) do
		if vim.fn.executable(command[1]) == 1 and run_system_command(command) and vim.fn.filereadable(output_path) == 1 then
			return output_path
		end
	end

	return nil, "No SVG rasterizer succeeded. Tried: rsvg-convert, magick, convert."
end

function M.path_is_renderable(path, config)
	local ext = util.file_extension(path)
	return util.extension_allowed(ext, config.image_extensions)
		or util.extension_allowed(ext, config.vector_extensions)
		or util.extension_allowed(ext, config.model_extensions)
end

function M.prepare_path_for_render(path, config)
	if not M.path_is_renderable(path, config) then
		return nil, "File type is not configured as renderable: " .. path
	end

	if util.extension_allowed(util.file_extension(path), config.vector_extensions) then
		return M.rasterize_svg(path)
	end

	return path
end

function M.buffer_is_renderable(target_buf, config)
	if not util.is_valid_buffer(target_buf) or vim.bo[target_buf].buftype ~= "" then
		return false
	end

	local path = vim.api.nvim_buf_get_name(target_buf)
	if path == "" or vim.fn.filereadable(path) ~= 1 then
		return false
	end

	return M.path_is_renderable(path, config)
end

return M
