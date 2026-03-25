local util = require("rndr.core.util")

local M = {}

local ns = vim.api.nvim_create_namespace("rndr_colors")
local hl_pool_size = 4096
local hl_group_by_color = {}
local hl_refcount_by_color = {}
local hl_active_colors_by_buf = {}
local hl_free_groups = {}

for index = hl_pool_size, 1, -1 do
	hl_free_groups[#hl_free_groups + 1] = "RndrColor_" .. index
end

local function release_buffer_highlights(buf)
	local active = hl_active_colors_by_buf[buf]
	if not active then
		return
	end

	for key in pairs(active) do
		local refcount = (hl_refcount_by_color[key] or 0) - 1
		if refcount <= 0 then
			hl_refcount_by_color[key] = nil
			local group = hl_group_by_color[key]
			if group then
				hl_group_by_color[key] = nil
				hl_free_groups[#hl_free_groups + 1] = group
			end
		else
			hl_refcount_by_color[key] = refcount
		end
	end

	hl_active_colors_by_buf[buf] = nil
end

local function highlight_group_for(fg_hex, bg_hex)
	if not util.is_hex_color(fg_hex) or not util.is_hex_color(bg_hex) or #fg_hex ~= 6 or #bg_hex ~= 6 then
		return nil
	end

	local normalized_fg = fg_hex:lower()
	local normalized_bg = bg_hex:lower()
	local key = normalized_fg .. ":" .. normalized_bg
	local group = hl_group_by_color[key]
	if group then
		return group, key
	end

	group = table.remove(hl_free_groups)
	if not group then
		return nil
	end

	vim.api.nvim_set_hl(0, group, { fg = "#" .. normalized_fg, bg = "#" .. normalized_bg })
	hl_group_by_color[key] = group

	return group, key
end

local function mark_buffer_highlight(buf, key)
	if not key then
		return
	end

	local active = hl_active_colors_by_buf[buf]
	if not active then
		active = {}
		hl_active_colors_by_buf[buf] = active
	end

	if active[key] then
		return
	end

	active[key] = true
	hl_refcount_by_color[key] = (hl_refcount_by_color[key] or 0) + 1
end

function M.preserve_source_buffer(state)
	local source_buf = state.source_buf
	if not util.is_valid_buffer(source_buf) or state.original_lines ~= nil then
		return
	end

	state.original_lines = vim.api.nvim_buf_get_lines(source_buf, 0, -1, false)
	state.original_modified = vim.bo[source_buf].modified
	state.original_modifiable = vim.bo[source_buf].modifiable
	state.original_readonly = vim.bo[source_buf].readonly
end

function M.restore_source_buffer(state)
	local source_buf = state and state.source_buf or nil
	if not util.is_valid_buffer(source_buf) or state.original_lines == nil then
		return
	end

	vim.bo[source_buf].readonly = false
	vim.bo[source_buf].modifiable = true
	vim.api.nvim_buf_set_lines(source_buf, 0, -1, false, state.original_lines)
	vim.bo[source_buf].modifiable = state.original_modifiable
	vim.bo[source_buf].readonly = state.original_readonly
	vim.bo[source_buf].modified = state.original_modified
	vim.api.nvim_buf_clear_namespace(source_buf, ns, 0, -1)
	release_buffer_highlights(source_buf)
	state.rendered = false
end

function M.apply_window_options(target_win, config)
	if not util.is_valid_window(target_win) then
		return
	end

	for option, value in pairs(config.win_options or {}) do
		vim.wo[target_win][option] = value
	end
end

function M.render_frame(state, rows)
	local source_buf = state.source_buf
	if not util.is_valid_buffer(source_buf) then
		return
	end

	local lines = {}
	for _, row in ipairs(rows) do
		table.insert(lines, row.text)
	end

	M.preserve_source_buffer(state)
	vim.bo[source_buf].readonly = false
	vim.bo[source_buf].modifiable = true
	vim.api.nvim_buf_set_lines(source_buf, 0, -1, false, lines)
	vim.bo[source_buf].modifiable = false
	vim.bo[source_buf].readonly = state.original_readonly
	vim.bo[source_buf].modified = false
	state.rendered = true

	vim.api.nvim_buf_clear_namespace(source_buf, ns, 0, -1)
	release_buffer_highlights(source_buf)

	for row_index, row in ipairs(rows) do
		local cell_count = vim.fn.strchars(row.text)
		local byte_offsets = {}
		for col = 0, cell_count do
			byte_offsets[col] = col == cell_count and #row.text or vim.fn.byteidx(row.text, col)
		end

		local span_start = nil
		local span_group = nil
		for col = 1, cell_count do
			local fg_hex = row.fg:sub((col - 1) * 6 + 1, col * 6)
			local bg_hex = row.bg:sub((col - 1) * 6 + 1, col * 6)
			local group, key = nil, nil
			if #fg_hex == 6 and #bg_hex == 6 then
				group, key = highlight_group_for(fg_hex, bg_hex)
			end
			if group then
				mark_buffer_highlight(source_buf, key)
			end

			if group ~= span_group then
				if span_group and span_start then
					vim.api.nvim_buf_add_highlight(
						source_buf,
						ns,
						span_group,
						row_index - 1,
						byte_offsets[span_start - 1],
						byte_offsets[col - 1]
					)
				end

				span_group = group
				span_start = group and col or nil
			end
		end

		if span_group and span_start then
			vim.api.nvim_buf_add_highlight(
				source_buf,
				ns,
				span_group,
				row_index - 1,
				byte_offsets[span_start - 1],
				byte_offsets[cell_count]
			)
		end
	end
end

return M
