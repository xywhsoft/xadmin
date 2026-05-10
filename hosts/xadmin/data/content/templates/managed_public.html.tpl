<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="description" content="内容展示页">
  <title>内容展示</title>
  <style>
    :root {
      --bg: #f5f7fb;
      --panel: #ffffff;
      --panel-strong: #ffffff;
      --ink: #243047;
      --muted: #667085;
      --line: #e7edf3;
      --accent: #1677ff;
      --accent-soft: #eef4ff;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: "Segoe UI", "PingFang SC", sans-serif;
      color: var(--ink);
      background: var(--bg);
    }
    .shell { max-width: 1320px; margin: 0 auto; padding: 20px; }
    .hero {
      display: grid;
      grid-template-columns: 1.3fr .7fr;
      gap: 18px;
      margin-bottom: 18px;
    }
    .hero-main, .hero-side, .panel {
      border-radius: 8px;
      border: 1px solid var(--line);
      background: var(--panel);
      box-shadow: 0 1px 3px rgba(16,24,40,.06);
    }
    .hero-main {
      padding: 22px;
      background: #fff;
    }
    .hero-main h1 { margin: 0 0 10px; font-size: 24px; }
    .hero-main p { margin: 0; max-width: 760px; color: var(--muted); line-height: 1.7; font-size: 14px; }
    .hero-note {
      margin-top: 12px;
      max-width: 760px;
      color: var(--muted);
      font-size: 13px;
      line-height: 1.6;
      white-space: pre-wrap;
    }
    .hero-meta {
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
      margin-top: 12px;
    }
    .hero-meta .chip {
      background: #eef4ff;
      color: var(--accent);
      border: 1px solid #d6e4ff;
    }
    .hero-side {
      padding: 22px;
      display: grid;
      gap: 12px;
      align-content: center;
    }
    .metric {
      padding: 14px 16px;
      border-radius: 6px;
      background: #fafcff;
      border: 1px solid var(--line);
    }
    .metric span {
      display: block;
      font-size: 12px;
      text-transform: none;
      letter-spacing: 0;
      color: var(--muted);
    }
    .metric strong {
      display: block;
      margin-top: 8px;
      font-size: 24px;
    }
    .metric small {
      display: block;
      margin-top: 6px;
      color: var(--muted);
      font-size: 11px;
      line-height: 1.5;
    }
    .slot-list {
      display: flex;
      flex-wrap: wrap;
      gap: 10px;
      margin-top: 6px;
    }
    .slot-card {
      padding: 12px 14px;
      border-radius: 6px;
      background: #fff;
      border: 1px solid var(--line);
    }
    .slot-card.mounted {
      border-color: #91caff;
      background: #f0f7ff;
    }
    .slot-card.configured {
      border-color: #ffd591;
      background: #fff7e6;
    }
    .slot-card.unresolved {
      border-color: #d0d5dd;
      background: #fff;
    }
    .slot-card.invalid {
      border-color: #ffccc7;
      background: #fff1f0;
    }
    .slot-card strong {
      display: block;
      font-size: 13px;
      margin-bottom: 6px;
    }
    .slot-card span {
      display: block;
      color: var(--muted);
      font-size: 12px;
      line-height: 1.5;
    }
    .slot-card a {
      display: inline-block;
      margin-top: 8px;
      color: #7dd3fc;
      text-decoration: none;
      font-size: 12px;
    }
    .layout { display: grid; grid-template-columns: 380px 1fr; gap: 18px; }
    .panel { padding: 18px; }
    .status { min-height: 18px; font-size: 13px; color: var(--accent); margin-bottom: 12px; }
    .list {
      display: flex;
      flex-direction: column;
      gap: 10px;
      max-height: 820px;
      overflow: auto;
    }
    .list.template-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      align-items: stretch;
    }
    .list.template-compact .item {
      padding: 10px 12px;
    }
    .list.template-compact .item-cover {
      display: none;
    }
    .list.template-feature .item:first-child {
      border-color: #91caff;
      background: #f0f7ff;
    }
    .list.template-feature .item:first-child strong {
      font-size: 18px;
    }
    .item {
      padding: 15px;
      border-radius: 6px;
      border: 1px solid transparent;
      background: #fff;
      cursor: pointer;
      transition: .15s ease;
    }
    .item.active {
      border-color: #91caff;
      background: #f0f7ff;
    }
    .item strong {
      display: block;
      margin-bottom: 8px;
      font-size: 16px;
    }
    .item-cover {
      width: 100%;
      height: 148px;
      object-fit: cover;
      border-radius: 6px;
      margin-bottom: 12px;
      border: 1px solid rgba(148,163,184,.18);
      background: #f5f7fb;
    }
    .item span {
      display: block;
      color: var(--muted);
      font-size: 12px;
      line-height: 1.55;
    }
    .chip-row {
      display: flex;
      gap: 8px;
      flex-wrap: wrap;
      margin: 8px 0 10px;
    }
    .chip {
      display: inline-flex;
      align-items: center;
      padding: 5px 10px;
      border-radius: 999px;
      background: var(--accent-soft);
      color: var(--accent);
      font-size: 12px;
      font-weight: 700;
    }
    .detail-title { margin: 0 0 8px; font-size: 30px; }
    .detail-meta { color: var(--muted); font-size: 13px; line-height: 1.6; margin-bottom: 16px; }
    .detail-cover {
      width: 100%;
      max-height: 320px;
      object-fit: cover;
      border-radius: 6px;
      margin-bottom: 18px;
      border: 1px solid var(--line);
      background: #f5f7fb;
    }
    .detail-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 12px;
    }
    .detail-card {
      padding: 15px;
      border-radius: 6px;
      background: var(--panel-strong);
      border: 1px solid var(--line);
    }
    .detail-card.full { grid-column: 1 / -1; }
    .detail-card span {
      display: block;
      font-size: 12px;
      text-transform: uppercase;
      letter-spacing: .04em;
      color: var(--muted);
      margin-bottom: 8px;
    }
    .detail-card div {
      line-height: 1.7;
      white-space: pre-wrap;
      word-break: break-word;
    }
    .detail-card pre {
      margin: 0;
      white-space: pre-wrap;
      word-break: break-word;
      font-family: Consolas, "Courier New", monospace;
      font-size: 12px;
      line-height: 1.7;
      color: #475467;
    }
    .detail-card .richtext :first-child { margin-top: 0; }
    .detail-card .richtext :last-child { margin-bottom: 0; }
    .native-abilities {
      margin-top: 18px;
      display: grid;
      gap: 12px;
    }
    .native-panel {
      border: 1px solid var(--line);
      border-radius: 6px;
      background: var(--panel-strong);
      padding: 14px;
    }
    .native-panel h3 {
      margin: 0 0 10px;
      font-size: 15px;
    }
    .native-metrics {
      display: flex;
      flex-wrap: wrap;
      gap: 10px;
      color: var(--muted);
      font-size: 13px;
    }
    .native-metrics strong {
      color: var(--text);
      margin-right: 4px;
    }
    .native-action {
      border: 0;
      border-radius: 4px;
      background: var(--accent);
      color: #fff;
      padding: 8px 14px;
      cursor: pointer;
    }
    .native-action.secondary {
      background: #f2f4f7;
      color: var(--text);
    }
    .native-form {
      display: grid;
      gap: 8px;
    }
    .native-form input,
    .native-form textarea {
      border: 1px solid var(--line);
      border-radius: 4px;
      padding: 9px 10px;
      font: inherit;
      background: #fff;
      color: var(--text);
    }
    .native-form textarea {
      min-height: 86px;
      resize: vertical;
    }
    .comment-list {
      display: grid;
      gap: 8px;
      margin-top: 12px;
    }
    .comment-item {
      border-top: 1px solid var(--line);
      padding-top: 8px;
    }
    .comment-item strong {
      display: block;
      margin-bottom: 4px;
      font-size: 13px;
    }
    .comment-item span {
      display: block;
      color: var(--muted);
      font-size: 12px;
      margin-bottom: 4px;
    }
    .comment-item p {
      margin: 0;
      white-space: pre-wrap;
      line-height: 1.6;
    }
    .rank-panel {
      margin-top: 14px;
      padding-top: 14px;
      border-top: 1px solid var(--line);
      display: none;
    }
    .rank-title {
      margin: 0 0 10px;
      font-size: 14px;
      font-weight: 700;
    }
    .rank-list {
      display: grid;
      gap: 8px;
    }
    .rank-item {
      width: 100%;
      border: 1px solid var(--line);
      border-radius: 6px;
      background: #fff;
      color: var(--ink);
      padding: 10px 12px;
      text-align: left;
      cursor: pointer;
    }
    .rank-item strong {
      display: block;
      font-size: 13px;
      margin-bottom: 5px;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }
    .rank-item span {
      display: block;
      color: var(--muted);
      font-size: 12px;
    }
    .detail-assets {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(120px, 1fr));
      gap: 10px;
    }
    .detail-assets.files {
      grid-template-columns: 1fr;
    }
    .detail-asset-image {
      display: block;
      border-radius: 16px;
      overflow: hidden;
      border: 1px solid var(--line);
      background: #f5f7fb;
    }
    .detail-asset-image img {
      display: block;
      width: 100%;
      height: 120px;
      object-fit: cover;
    }
    .detail-asset-file {
      display: flex;
      align-items: center;
      gap: 8px;
      padding: 12px 14px;
      border-radius: 14px;
      border: 1px solid var(--line);
      color: var(--ink);
      text-decoration: none;
      background: #fafcff;
      word-break: break-all;
    }
    .detail-asset-file:hover {
      border-color: rgba(96,165,250,.45);
      color: var(--accent);
    }
    .workspace-shell {
      margin-top: 18px;
      border-radius: 6px;
      border: 1px solid var(--line);
      background: var(--panel-strong);
      overflow: hidden;
    }
    .workspace-toolbar {
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 10px;
      flex-wrap: wrap;
      padding: 14px 16px;
      border-bottom: 1px solid var(--line);
      background: #fafcff;
    }
    .workspace-title {
      font-size: 14px;
      font-weight: 700;
    }
    .workspace-toolbar .toolbar {
      display: flex;
      gap: 10px;
      flex-wrap: wrap;
    }
    .workspace-button {
      border: 0;
      border-radius: 4px;
      padding: 9px 14px;
      font-weight: 700;
      cursor: pointer;
      background: #fff;
      color: var(--ink);
      border: 1px solid #d0d5dd;
    }
    .workspace-button.danger {
      background: #fff1f0;
      color: #cf1322;
      border-color: #ffccc7;
    }
    .searchbar {
      display: flex;
      gap: 10px;
      flex-wrap: wrap;
      margin-bottom: 12px;
    }
    .searchbar input {
      flex: 1 1 180px;
      min-height: 42px;
      border-radius: 4px;
      border: 1px solid var(--line);
      background: #fff;
      color: var(--ink);
      padding: 0 14px;
      outline: none;
    }
    .searchbar select {
      min-height: 42px;
      border-radius: 4px;
      border: 1px solid var(--line);
      background: #fff;
      color: var(--ink);
      padding: 0 14px;
      outline: none;
    }
    .searchbar button {
      border: 0;
      border-radius: 4px;
      padding: 10px 16px;
      font-weight: 700;
      cursor: pointer;
      background: #fff;
      color: var(--ink);
      border: 1px solid #d0d5dd;
    }
    .workspace-frame {
      width: 100%;
      min-height: 560px;
      border: 0;
      background: #fff;
    }
    .workspace-empty {
      min-height: 220px;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 24px;
      color: var(--muted);
      text-align: center;
      line-height: 1.7;
    }
    .empty {
      padding: 42px 18px;
      text-align: center;
      color: var(--muted);
    }
    @media (max-width: 1120px) {
      .hero, .layout, .detail-grid { grid-template-columns: 1fr; }
    }
    @media (max-width: 760px) {
      .hero-main h1 { font-size: 30px; }
      .detail-grid { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="shell">
    <section class="hero">
      <div class="hero-main">
        <h1 id="mp_title">内容展示</h1>
        <p id="mp_desc">内容模型驱动的公共展示层。列表与详情字段跟随生成插件的 spec 变化，优先复用模型本身的展示约束。</p>
        <div class="hero-note" id="mp_meta_note"></div>
        <div class="hero-meta" id="mp_meta_tags"></div>
      </div>
      <div class="hero-side">
        <div class="metric"><span>插件</span><strong id="mp_metric_plugin">{{PLUGIN_XID}}</strong></div>
        <div class="metric"><span>已发布记录</span><strong id="mp_metric_count">0</strong></div>
        <div class="metric"><span>详情字段</span><strong id="mp_metric_fields">0</strong></div>
        <div class="metric"><span>能力槽位</span><strong id="mp_metric_slots">0</strong><small id="mp_metric_slots_detail">还没有加载能力契约。</small></div>
        <div class="slot-list" id="mp_slot_list"><div class="slot-card"><strong>还没有能力槽位</strong><span>后续能力包可以通过已声明契约挂载进来。</span></div></div>
      </div>
    </section>
    <div class="layout">
      <aside class="panel">
        <div class="searchbar">
          <input id="mp_search" type="text" placeholder="搜索标题、摘要、Slug..." onkeydown="mpHandleSearchKey(event)">
          <select id="mp_sort_by" onchange="mpApplySearch()"></select>
          <select id="mp_sort_dir" onchange="mpApplySearch()">
            <option value="">默认方向</option>
            <option value="desc">降序</option>
            <option value="asc">升序</option>
          </select>
          <button type="button" onclick="mpApplySearch()">搜索</button>
        </div>
        <div class="searchbar" id="mp_field_filters"></div>
        <div class="searchbar" id="mp_taxonomy_filters"></div>
        <div class="status" id="mp_nav_status"></div>
        <div class="list" id="mp_list"><div class="empty">加载中...</div></div>
        <section class="rank-panel" id="mp_rank_panel">
          <h2 class="rank-title">访问排行</h2>
          <div class="rank-list" id="mp_rank_list"><div class="empty">暂无排行。</div></div>
        </section>
      </aside>
      <main class="panel">
        <div class="status" id="mp_detail_status"></div>
        <div id="mp_detail"><div class="empty">请选择一条记录查看详情。</div></div>
      </main>
    </div>
  </div>

  <script>
    const mp = {
      meta: null,
      items: [],
      currentId: 0,
      currentItem: null,
      query: "",
      sortBy: "",
      sortDir: "",
      categoryId: "",
      tagId: "",
      topicId: "",
      categories: [],
      tags: [],
      topics: [],
      ranks: [],
      abilityState: {},
      fieldFilters: {},
      listMeta: {},
      workspaceSlotKey: "",
      workspaceHref: "",
      workspaceTitle: ""
    };

    function mpEscape(value) {
      return String(value || "").replace(/[&<>\"']/g, (ch) => {
        if (ch === "&") return "&amp;";
        if (ch === "<") return "&lt;";
        if (ch === ">") return "&gt;";
        if (ch === "\"") return "&quot;";
        return "&#39;";
      });
    }

    function mpSetStatus(id, message, color) {
      const el = document.getElementById(id);
      el.innerText = message || "";
      el.style.color = color || "#7dd3fc";
    }

    function mpHandleError(err) {
      const message = err && err.message ? err.message : String(err);
      mpSetStatus("mp_nav_status", message, "#f87171");
      mpSetStatus("mp_detail_status", message, "#f87171");
      return message;
    }

    function mpFields() {
      return (((mp.meta || {}).spec || {}).entity || {}).fields || [];
    }

    function mpListFields() {
      const configured = (((((mp.meta || {}).spec || {}).ui || {}).list || {}).columns) || [];
      const fields = mpFields().filter((field) => field && field.name);
      const fieldMap = {};
      if (!Array.isArray(configured) || !configured.length) {
        return fields.filter((field) => field.showInList !== false);
      }
      for (const field of fields) {
        fieldMap[field.name] = field;
      }
      return configured.map((name) => fieldMap[name]).filter((field) => field && field.showInList !== false);
    }

    function mpDetailFields() {
      return mpFields().filter((field) => field && field.showInDetail !== false);
    }

    function mpMetadata() {
      return ((((mp.meta || {}).spec || {}).metadata) || {});
    }

    function mpPolicies() {
      return ((((mp.meta || {}).spec || {}).policies) || {});
    }

    function mpMetadataItems() {
      const metadata = mpMetadata();
      const labels = Array.isArray(metadata.labels) ? metadata.labels.filter(Boolean).map((item) => `label:${item}`) : [];
      const tags = Array.isArray(metadata.tags) ? metadata.tags.filter(Boolean).map((item) => `tag:${item}`) : [];
      const policies = mpPolicies();
      const policyItems = [];
      if (policies.statusFlow) policyItems.push(`statusFlow:${policies.statusFlow}`);
      if (policies.authorMode) policyItems.push(`authorMode:${policies.authorMode}`);
      if (policies.publishMode) policyItems.push(`publishMode:${policies.publishMode}`);
      if (policies.deleteMode) policyItems.push(`deleteMode:${policies.deleteMode}`);
      return [...labels, ...tags, ...policyItems];
    }

    function mpMetadataNotes() {
      const notes = mpMetadata().notes;
      return typeof notes === "string" ? notes.trim() : "";
    }

    function mpRenderHeroMetadata() {
      const host = document.getElementById("mp_meta_tags");
      const items = mpMetadataItems();
      if (!host) return;
      host.innerHTML = items.map((item) => `<span class="chip">${mpEscape(item)}</span>`).join("");
      host.style.display = items.length ? "flex" : "none";
      const noteEl = document.getElementById("mp_meta_note");
      const note = mpMetadataNotes();
      if (noteEl) {
        noteEl.innerText = note;
        noteEl.style.display = note ? "block" : "none";
      }
    }

    function mpDetailConfig() {
      return (((((mp.meta || {}).spec || {}).ui || {}).detail) || {});
    }

    function mpDetailShowPublishedAt() {
      return mpDetailConfig().showPublishedAt !== false;
    }

    function mpFindFieldByRole(role) {
      const target = String(role || "").toLowerCase();
      if (!target) return null;
      return mpFields().find((field) => String((((field || {}).semantic || {}).role || "")).toLowerCase() === target) || null;
    }

    function mpDetailAuthorField() {
      return mpFindFieldByRole("author");
    }

    function mpDetailShowAuthor() {
      return mpDetailConfig().showAuthor !== false && !!mpDetailAuthorField();
    }

    function mpDetailAuthorText(item) {
      const field = mpDetailAuthorField();
      if (!field || !item || !item.data) return "";
      return mpDisplayText(field, item.data[field.name]);
    }

    function mpListPageSize() {
      const pageSize = Number((((((mp.meta || {}).spec || {}).ui || {}).list || {}).pageSize) || 0);
      if (!Number.isFinite(pageSize) || pageSize <= 0) return 100;
      return Math.min(pageSize, 200);
    }

    function mpStatusFlow() {
      return String((mpPolicies().statusFlow || "draft-published")).trim() || "draft-published";
    }

    function mpStatusOptions() {
      return mpStatusFlow() === "draft-review-published"
        ? [
            { value: 0, label: "隐藏" },
            { value: 1, label: "待审核" },
            { value: 2, label: "已发布" }
          ]
        : [
            { value: 0, label: "隐藏" },
            { value: 1, label: "已发布" }
          ];
    }

    function mpStatusField() {
      return mpFields().find((field) => {
        if (!field || !field.name) return false;
        const role = String((((field || {}).semantic || {}).role) || "").toLowerCase();
        const configured = String(((((mp.meta || {}).spec || {}).entity || {}).statusField) || "");
        return role === "status" || (configured && field.name === configured);
      }) || null;
    }

    function mpFilterableFields() {
      return mpFields().filter((field) => field && field.name && field.filterable);
    }

    function mpFieldStorageType(field) {
      return String((((field || {}).storage || {}).type || "")).toLowerCase();
    }

    function mpFieldComponentType(field) {
      return String((((field || {}).component || {}).type || "")).toLowerCase();
    }

    function mpFieldQueryKey(field) {
      return `f_${(field && field.name) || ""}`;
    }

    function mpFieldFilterId(field) {
      return `mp_filter_field_${(field && field.name) || ""}`;
    }

    function mpFieldFilterValue(name) {
      const source = mp.fieldFilters || {};
      return Object.prototype.hasOwnProperty.call(source, name) ? String(source[name] || "") : "";
    }

    function mpFieldFilterControl(field) {
      const componentType = mpFieldComponentType(field);
      const storageType = mpFieldStorageType(field);
      const options = mpFieldOptions(field);
      const id = mpFieldFilterId(field);
      const label = field.title || field.name;

      if (Array.isArray(options) && options.length) {
        const optionHtml = options.map((item) => {
          if (item && typeof item === "object" && "value" in item) {
            return `<option value="${mpEscape(item.value)}">${mpEscape(item.label || item.title || item.text || item.value)}</option>`;
          }
          return `<option value="${mpEscape(item)}">${mpEscape(item)}</option>`;
        }).join("");
        return `<select id="${mpEscape(id)}" onchange="mpApplySearch()"><option value="">全部${mpEscape(label)}</option>${optionHtml}</select>`;
      }
      if (componentType === "switch" || storageType === "bool") {
        return `<select id="${mpEscape(id)}" onchange="mpApplySearch()"><option value="">全部${mpEscape(label)}</option><option value="true">启用</option><option value="false">停用</option></select>`;
      }
      return `<input id="${mpEscape(id)}" type="text" placeholder="${mpEscape(label)}" onkeydown="mpHandleSearchKey(event)">`;
    }

    function mpRenderFieldFilters() {
      const host = document.getElementById("mp_field_filters");
      const fields = mpFilterableFields();
      if (!host) return;
      if (!fields.length) {
        host.innerHTML = "";
        return;
      }
      host.innerHTML = fields.map((field) => mpFieldFilterControl(field)).join("");
      for (const field of fields) {
        const el = document.getElementById(mpFieldFilterId(field));
        if (el) el.value = mpFieldFilterValue(field.name);
      }
    }

    function mpReadFieldFiltersFromDom() {
      const next = {};
      for (const field of mpFilterableFields()) {
        const el = document.getElementById(mpFieldFilterId(field));
        const value = String((el || {}).value || "").trim();
        if (value !== "") {
          next[field.name] = value;
        }
      }
      return next;
    }

    function mpHasAbilityPack(packId) {
      const packs = (((mp.meta || {}).contracts || {}).abilityPacks) || [];
      return packs.some((item) => String((item || {}).packId || "") === String(packId));
    }

    function mpAbilityPackConfig(packId) {
      const packs = (((mp.meta || {}).contracts || {}).abilityPacks) || [];
      const pack = packs.find((item) => String((item || {}).packId || "") === String(packId));
      return (pack && pack.instanceConfig && typeof pack.instanceConfig === "object") ? pack.instanceConfig : {};
    }

    function mpAbilityRoutePrefix(packId, name, fallback) {
      const config = mpAbilityPackConfig(packId);
      let prefix = String((config && config[name]) || fallback || "");
      prefix = prefix.replace(/\{pluginXid\}/g, "{{PLUGIN_XID}}").trim();
      if (!prefix || prefix[0] !== "/") prefix = String(fallback || "");
      while (prefix.length > 1 && prefix.endsWith("/")) {
        prefix = prefix.slice(0, -1);
      }
      return prefix || String(fallback || "");
    }

    function mpApplyTextTemplate(template, values) {
      const text = String(template || "");
      if (!text) return "";
      return text.replace(/\{([A-Za-z0-9_]+)\}/g, (all, key) => {
        return Object.prototype.hasOwnProperty.call(values || {}, key) ? String(values[key] || "") : all;
      });
    }

    function mpClientKey(name) {
      const key = `managed_public_${name}_{{PLUGIN_XID}}`;
      try {
        let value = window.localStorage.getItem(key);
        if (!value) {
          value = `${name}_${Date.now()}_${Math.random().toString(16).slice(2)}`;
          window.localStorage.setItem(key, value);
        }
        return value;
      } catch (err) {
        return `${name}_anonymous`;
      }
    }

    function mpRenderTaxonomyFilters() {
      const host = document.getElementById("mp_taxonomy_filters");
      const parts = [];
      if (!host) return;
      if (mpHasAbilityPack("content.category")) {
        parts.push(`<select id="mp_category_filter" onchange="mpApplySearch()"><option value="">All categories</option>${(mp.categories || []).map((item) => `<option value="${mpEscape(item.id)}">${mpEscape(item.breadcrumb || item.treeTitle || item.title || item.slug || item.id)}</option>`).join("")}</select>`);
      }
      if (mpHasAbilityPack("content.tag")) {
        parts.push(`<select id="mp_tag_filter" onchange="mpApplySearch()"><option value="">全部标签</option>${(mp.tags || []).map((item) => `<option value="${mpEscape(item.id)}">${mpEscape(item.name || item.slug || item.id)}</option>`).join("")}</select>`);
      }
      if (mpHasAbilityPack("content.topic")) {
        parts.push(`<select id="mp_topic_filter" onchange="mpApplySearch()"><option value="">全部专题</option>${(mp.topics || []).map((item) => `<option value="${mpEscape(item.id)}">${mpEscape(item.title || item.slug || item.id)}</option>`).join("")}</select>`);
      }
      host.innerHTML = parts.join("");
      const categoryEl = document.getElementById("mp_category_filter");
      const tagEl = document.getElementById("mp_tag_filter");
      const topicEl = document.getElementById("mp_topic_filter");
      if (categoryEl) categoryEl.value = mp.categoryId || "";
      if (tagEl) tagEl.value = mp.tagId || "";
      if (topicEl) topicEl.value = mp.topicId || "";
    }

    function mpApplyUrlState() {
      const params = new URLSearchParams(window.location.search);
      const nextFieldFilters = {};
      mp.query = String(params.get("q") || "");
      mp.sortBy = String(params.get("sortBy") || "");
      mp.sortDir = String(params.get("sortDir") || "");
      mp.categoryId = String(params.get("categoryId") || "");
      mp.tagId = String(params.get("tagId") || "");
      mp.topicId = String(params.get("topicId") || "");
      for (const field of mpFilterableFields()) {
        const value = String(params.get(mpFieldQueryKey(field)) || "").trim();
        if (value !== "") {
          nextFieldFilters[field.name] = value;
        }
      }
      mp.fieldFilters = nextFieldFilters;
      const searchEl = document.getElementById("mp_search");
      if (searchEl) searchEl.value = mp.query || "";
    }

    function mpPrettySlugFromPath() {
      const prefix = `${mpAbilityRoutePrefix("content.slug", "slugRoutePrefix", "/{{PLUGIN_XID}}")}/`;
      const path = String(window.location.pathname || "");
      if (!mpHasAbilityPack("content.slug") || path.indexOf(prefix) !== 0) return "";
      const slug = path.slice(prefix.length);
      if (!slug || slug.indexOf("/") >= 0) return "";
      try {
        return decodeURIComponent(slug);
      } catch (err) {
        return slug;
      }
    }

    function mpSyncUrl(item) {
      const params = new URLSearchParams();
      const prettySlug = mpPrettySlugFromPath();
      if (mp.query) params.set("q", mp.query);
      if (mp.sortBy) params.set("sortBy", mp.sortBy);
      if (mp.sortDir) params.set("sortDir", mp.sortDir);
      if (mp.categoryId) params.set("categoryId", mp.categoryId);
      if (mp.tagId) params.set("tagId", mp.tagId);
      if (mp.topicId) params.set("topicId", mp.topicId);
      for (const field of mpFilterableFields()) {
        const value = String(((mp.fieldFilters || {})[field.name] || "")).trim();
        if (value !== "") {
          params.set(mpFieldQueryKey(field), value);
        }
      }
      if (item && item.slug && String(item.slug) !== prettySlug) {
        params.set("slug", String(item.slug));
      } else if (item && Number(item.id) > 0 && !prettySlug) {
        params.set("id", String(item.id));
      }
      const next = `${window.location.pathname}${params.toString() ? `?${params.toString()}` : ""}`;
      window.history.replaceState({}, "", next);
    }

    function mpEnsureMeta(name) {
      let meta = document.querySelector(`meta[name="${name}"]`);
      if (!meta) {
        meta = document.createElement("meta");
        meta.setAttribute("name", name);
        document.head.appendChild(meta);
      }
      return meta;
    }

    function mpEnsureCanonical() {
      let link = document.querySelector('link[rel="canonical"]');
      if (!link) {
        link = document.createElement("link");
        link.setAttribute("rel", "canonical");
        document.head.appendChild(link);
      }
      return link;
    }

    function mpUpdateHead(item, seoMeta) {
      const identity = (((mp.meta || {}).spec || {}).identity) || {};
      const pluginTitle = (mp.meta && mp.meta.title) || identity.title || "{{PLUGIN_XID}}";
      const category = !item ? mpSelectedCategory() : null;
      const seo = seoMeta || mpCategorySeoMeta(category) || null;
      const title = (seo && seo.title) || (item && item.title ? `${item.title} - ${pluginTitle}` : (category && category.title ? `${category.title} - ${pluginTitle}` : pluginTitle));
      const summary = (seo && seo.description) || (item && item.summary) || (category && category.description) || identity.description || "内容展示页";
      const keywords = (seo && seo.keywords) || "";
      const canonical = (seo && seo.canonical) || "";
      document.title = title;
      const metaDesc = mpEnsureMeta("description");
      if (metaDesc) metaDesc.setAttribute("content", summary);
      if (keywords) mpEnsureMeta("keywords").setAttribute("content", keywords);
      if (canonical) mpEnsureCanonical().setAttribute("href", canonical);
    }

    async function mpLoadSeoMeta(item) {
      if (!item || !item.id || !mpHasAbilityPack("content.seo")) return null;
      if (item.seoMeta) return item.seoMeta;
      const params = new URLSearchParams();
      if (item.slug) params.set("slug", String(item.slug));
      else params.set("id", String(item.id));
      const result = await mpFetchJson(`/api/plugin/{{PLUGIN_XID}}/seo/meta?${params.toString()}`, { cache: "no-store" });
      return (result && result.data) || null;
    }

    function mpSortOptions() {
      const options = [
        { value: "", label: "默认排序" },
        { value: "updateTime", label: "更新时间" },
        { value: "createTime", label: "创建时间" },
        { value: "title", label: "标题" },
        { value: "publishedAt", label: "发布时间" },
        { value: "id", label: "ID" }
      ];
      for (const field of mpFields()) {
        if (!field || !field.name || !field.sortable) continue;
        if (options.some((item) => item.value === field.name)) continue;
        options.push({ value: field.name, label: field.title || field.name });
      }
      return options;
    }

    function mpSelectedCategory() {
      const id = Number(mp.categoryId || 0);
      if (!id) return null;
      return (mp.categories || []).find((item) => Number(item.id || 0) === id) || null;
    }

    function mpCategorySeoMeta(category) {
      if (!category || !mpHasAbilityPack("content.seo")) return null;
      const identity = (((mp.meta || {}).spec || {}).identity) || {};
      const pluginTitle = (mp.meta && mp.meta.title) || identity.title || "{{PLUGIN_XID}}";
      const config = mpAbilityPackConfig("content.seo");
      const values = {
        categoryId: category.id || "",
        categoryTitle: category.title || "",
        categorySlug: category.slug || "",
        categoryDescription: category.description || "",
        siteName: pluginTitle,
        pluginTitle: pluginTitle,
        pluginXid: "{{PLUGIN_XID}}"
      };
      const title = category.seoTitle || mpApplyTextTemplate(config.categoryTitleTemplate, values) || category.title || "";
      const keywords = category.seoKeywords || mpApplyTextTemplate(config.categoryKeywordsTemplate, values) || "";
      const description = category.seoDescription || mpApplyTextTemplate(config.categoryDescriptionTemplate, values) || category.description || "";
      const canonical = mpApplyTextTemplate(config.categoryCanonicalTemplate, values) || (category.slug ? `/plugin/{{PLUGIN_XID}}?categoryId=${encodeURIComponent(category.id)}` : "");
      if (!title && !keywords && !description && !canonical) return null;
      return {
        title: title,
        keywords: keywords,
        description: description,
        canonical: canonical
      };
    }

    function mpCategoryTemplateKey() {
      const category = mpSelectedCategory();
      const raw = String((category && category.templateKey) || "default").trim().toLowerCase();
      return /^(compact|grid|feature)$/.test(raw) ? raw : "default";
    }

    function mpRenderListControls() {
      const sortEl = document.getElementById("mp_sort_by");
      if (sortEl) {
        sortEl.innerHTML = mpSortOptions().map((item) => `<option value="${mpEscape(item.value)}">${mpEscape(item.label)}</option>`).join("");
        sortEl.value = mp.sortBy || "";
      }
      const dirEl = document.getElementById("mp_sort_dir");
      if (dirEl) dirEl.value = mp.sortDir || "";
      mpRenderFieldFilters();
      mpRenderTaxonomyFilters();
    }

    function mpApplySearch() {
      mp.query = String((document.getElementById("mp_search") || {}).value || "").trim();
      mp.sortBy = String((document.getElementById("mp_sort_by") || {}).value || "").trim();
      mp.sortDir = String((document.getElementById("mp_sort_dir") || {}).value || "").trim();
      mp.categoryId = String((document.getElementById("mp_category_filter") || {}).value || "").trim();
      mp.tagId = String((document.getElementById("mp_tag_filter") || {}).value || "").trim();
      mp.topicId = String((document.getElementById("mp_topic_filter") || {}).value || "").trim();
      mp.fieldFilters = mpReadFieldFiltersFromDom();
      mpLoadList().then(async () => {
        if (mp.items.length) {
          await mpLoadDetail(mp.items[0].id);
        } else {
          mpRenderDetail(null);
          mpSyncUrl(null);
          mpUpdateHead(null, mpCategorySeoMeta(mpSelectedCategory()));
        }
      }).catch((err) => {
        mpSetStatus("mp_nav_status", err && err.message ? err.message : String(err), "#f87171");
      });
    }

    function mpHandleSearchKey(event) {
      if (event && event.key === "Enter") {
        mpApplySearch();
      }
    }

    function mpCapabilitySlots() {
      const mountItems = (((mp.meta || {}).mounts || {}).mounts) || [];
      if (mountItems.length) return mountItems.map((item) => item.slot || {});
      return ((((mp.meta || {}).managed || {}).capabilitySlots) || []);
    }

    function mpMountRegistry() {
      return ((mp.meta || {}).mounts) || { mounts: [] };
    }

    function mpProviderDiscovery() {
      return ((mp.meta || {}).providerDiscovery) || { providers: [], slots: [] };
    }

    function mpProviderCatalog() {
      return (mpProviderDiscovery().providers || []).filter(Boolean);
    }

    function mpProviderMatches(provider, providerPlugin, serviceXid) {
      if (!provider) return false;
      if (providerPlugin) {
        return provider.pluginXid === providerPlugin
          || provider.key === providerPlugin
          || (provider.serviceXidList || []).some((item) => String(item) === String(providerPlugin));
      }
      if (serviceXid) {
        return (provider.serviceXidList || []).some((item) => String(item) === String(serviceXid))
          || (((provider.defaults || {}).serviceXid) || "") === serviceXid;
      }
      return false;
    }

    function mpResolveProviderDefinition(slotKey, providerPlugin, serviceXid) {
      const catalog = mpProviderCatalog();
      if (!catalog.length) return null;
      if (providerPlugin || serviceXid) {
        return catalog.find((provider) => mpProviderMatches(provider, providerPlugin, serviceXid)) || null;
      }
      const item = mpFindMountItem(slotKey);
      const slotService = (((item || {}).slot || {}).serviceXid) || "";
      if (slotService) {
        return catalog.find((provider) => mpProviderMatches(provider, "", slotService)) || null;
      }
      return null;
    }

    function mpProviderOptionSchema(provider) {
      return provider
        ? (provider.optionSchema || provider.mountOptionsSchema || provider.configSchema || provider.mountSchema || null)
        : null;
    }

    function mpProviderOptionField(provider, name) {
      const schema = mpProviderOptionSchema(provider);
      if (!schema || !Array.isArray(schema.groups)) return null;
      for (const group of schema.groups) {
        for (const field of ((group || {}).fields || [])) {
          if (field && field.name === name) return field;
        }
      }
      return null;
    }

    function mpCurrentContentContext() {
      if (!mp.currentItem || Number(mp.currentItem.id) <= 0) return null;
      return {
        contentPlugin: ((mp.meta || {}).pluginXid) || "{{PLUGIN_XID}}",
        contentId: Number(mp.currentItem.id)
      };
    }

    function mpBuildScopedHref(baseHref) {
      const context = mpCurrentContentContext();
      if (!baseHref) return "";
      try {
        const url = new URL(baseHref, window.location.origin);
        if (context) {
          url.searchParams.set("contentPlugin", context.contentPlugin);
          url.searchParams.set("contentId", String(context.contentId));
        }
        return url.origin === window.location.origin
          ? `${url.pathname}${url.search}${url.hash}`
          : url.toString();
      } catch (err) {
        return baseHref;
      }
    }

    function mpBuildMountHref(item, baseHref) {
      const mount = ((item || {}).mount) || {};
      const options = mount.options || {};
      const slotKey = ((item || {}).slot || {}).key || (item || {}).slotKey || "";
      const href = mpBuildScopedHref(baseHref);
      if (!href) return "";
      try {
        const url = new URL(href, window.location.origin);
        if (slotKey) url.searchParams.set("mountSlotKey", slotKey);
        if (options && Object.keys(options).length) {
          url.searchParams.set("mountOptions", JSON.stringify(options));
        }
        return url.origin === window.location.origin
          ? `${url.pathname}${url.search}${url.hash}`
          : url.toString();
      } catch (err) {
        return href;
      }
    }

    function mpCurrentScopeText() {
      const context = mpCurrentContentContext();
      if (!context) return "选择一条详情记录后，外部能力会自动绑定到当前内容。";
      return `已绑定到 ${context.contentPlugin} #${context.contentId}。`;
    }

    function mpFindMountItem(slotKey) {
      return (mpMountRegistry().mounts || []).find((item) => {
        const key = ((item || {}).slot || {}).key || (item || {}).slotKey || "";
        return key === slotKey;
      }) || null;
    }

    function mpMountPublicHref(item) {
      const mount = (item || {}).mount || {};
      const publicEntry = mount.publicEntry || {};
      return mpBuildMountHref(item, publicEntry.href || "");
    }

    function mpRenderWorkspace() {
      const host = document.getElementById("mp_workspace_host");
      const titleEl = document.getElementById("mp_workspace_title");
      const scopeEl = document.getElementById("mp_workspace_scope");
      if (!host || !titleEl || !scopeEl) return;
      if (!mp.workspaceHref) {
        titleEl.innerText = "尚未打开能力面板";
        scopeEl.innerText = "选择一个已挂载槽位后即可在这里打开前台能力。";
        host.innerHTML = "<div class='workspace-empty'>选择一个已挂载槽位后即可在这里打开前台能力。</div>";
        return;
      }
      titleEl.innerText = mp.workspaceTitle || mp.workspaceSlotKey || "已挂载能力";
      scopeEl.innerText = mpCurrentScopeText();
      host.innerHTML = `<iframe class="workspace-frame" src="${mpEscape(mp.workspaceHref)}" loading="lazy"></iframe>`;
    }

    function mpSyncWorkspaceForContext() {
      if (!mp.workspaceSlotKey) {
        mpRenderWorkspace();
        return;
      }
      const item = mpFindMountItem(mp.workspaceSlotKey);
      const href = mpMountPublicHref(item);
      if (!href) {
        mp.workspaceHref = "";
        mp.workspaceTitle = "";
        mpSetStatus("mp_detail_status", `槽位 ${mp.workspaceSlotKey} 对应的挂载能力已不可用。`, "#f87171");
        mpRenderWorkspace();
        return;
      }
      mp.workspaceHref = href;
      mp.workspaceTitle = (((item || {}).mount || {}).publicEntry || {}).title || (((item || {}).slot || {}).title) || mp.workspaceSlotKey;
      mpRenderWorkspace();
    }

    function mpOpenWorkspace(slotKey) {
      const item = mpFindMountItem(slotKey);
      const href = mpMountPublicHref(item);
      if (!href) {
        mpSetStatus("mp_detail_status", `槽位 ${slotKey} 没有可打开的前台入口。`, "#f87171");
        return;
      }
      mp.workspaceSlotKey = slotKey || "";
      mp.workspaceHref = href;
      mp.workspaceTitle = (((item || {}).mount || {}).publicEntry || {}).title || (((item || {}).slot || {}).title) || slotKey || "已挂载能力";
      mpRenderWorkspace();
      mpSetStatus("mp_detail_status", `已内嵌打开 ${mp.workspaceTitle}。`);
    }

    function mpRefreshWorkspace() {
      if (!mp.workspaceSlotKey) {
        mpSetStatus("mp_detail_status", "当前没有内嵌打开的挂载能力。");
        mpRenderWorkspace();
        return;
      }
      mpSyncWorkspaceForContext();
      if (mp.workspaceHref) {
        mpSetStatus("mp_detail_status", `槽位 ${mp.workspaceSlotKey} 的工作区已刷新。`);
      }
    }

    function mpOpenWorkspaceNewTab() {
      if (!mp.workspaceHref) {
        mpSetStatus("mp_detail_status", "当前没有内嵌打开的挂载能力。");
        return;
      }
      window.open(mp.workspaceHref, "_blank", "noopener,noreferrer");
    }

    function mpCloseWorkspace() {
      mp.workspaceSlotKey = "";
      mp.workspaceHref = "";
      mp.workspaceTitle = "";
      mpRenderWorkspace();
      mpSetStatus("mp_detail_status", "工作区已关闭。");
    }

    function mpSlotStateClass(status) {
      if (status === "mounted") return " mounted";
      if (status === "configured") return " configured";
      if (status === "unresolved") return " unresolved";
      return " invalid";
    }

    function mpSlotStateText(status) {
      switch (status) {
        case "mounted": return "已挂载";
        case "configured": return "已配置";
        case "unresolved": return "待处理";
        case "invalid-slot": return "槽位无效";
        case "invalid-provider": return "提供方无效";
        case "provider-mismatch": return "提供方不匹配";
        case "surface-mismatch": return "挂载面不匹配";
        case "invalid-route": return "路由无效";
        default: return status || "未知";
      }
    }

    function mpMountSummaryText(registry, slotCount) {
      const declared = Number((registry || {}).declaredCount || slotCount || 0);
      const configured = Number((registry || {}).configuredCount || 0);
      const unresolved = Number((registry || {}).unresolvedCount || 0);
      const invalid = Number((registry || {}).invalidCount || 0);
      const extras = Math.max(Number((registry || {}).total || 0) - declared, 0);
      const parts = [];
      if (configured) parts.push(`${configured} 个已配置`);
      if (unresolved) parts.push(`${unresolved} 个待处理`);
      if (invalid) parts.push(`${invalid} 个异常`);
      if (extras) parts.push(`${extras} 个额外挂载项`);
      return parts.join(" · ") || "所有声明的槽位都已挂载。";
    }

    function mpFieldOptions(field) {
      return field.list
        || field.options
        || ((field.component || {}).list)
        || ((field.component || {}).options)
        || (((field.component || {}).props || {}).list)
        || (((field.component || {}).props || {}).options)
        || ((() => {
          const role = String((((field || {}).semantic || {}).role) || "").toLowerCase();
          const statusField = mpStatusField();
          if (role === "status" || (statusField && statusField.name === (field || {}).name)) {
            return mpStatusOptions();
          }
          return null;
        })())
        || [];
    }

    function mpFieldLabel(field, value) {
      const options = mpFieldOptions(field);
      for (const item of options) {
        if (!item) continue;
        if (typeof item === "object" && "value" in item) {
          if (String(item.value) === String(value)) return item.label || item.title || item.text || item.value;
        } else if (String(item) === String(value)) {
          return item;
        }
      }
      return value;
    }

    function mpDisplayText(field, value) {
      if (value == null || value === "") return "";
      const componentType = String((((field || {}).component || {}).type || "")).toLowerCase();
      if (componentType === "image" || componentType === "images" || componentType === "file" || componentType === "files") {
        const items = mpAssetValues(value);
        if (!items.length) return "";
        if (componentType === "image") return mpAssetLabel(items[0]) || "图片";
        if (componentType === "file") return mpAssetLabel(items[0]) || "文件";
        if (componentType === "images") return items.length === 1 ? (mpAssetLabel(items[0]) || "1 张图片") : `${items.length} 张图片`;
        return items.length === 1 ? (mpAssetLabel(items[0]) || "1 个文件") : `${items.length} 个文件`;
      }
      if (Array.isArray(value)) {
        return value.map((item) => mpDisplayText(field, item)).filter(Boolean).join(", ");
      }
      if (componentType === "switch" || typeof value === "boolean") {
        return value ? "启用" : "停用";
      }
      if (componentType === "select" || componentType === "radio" || componentType === "checkbox" || componentType === "checklist") {
        return String(mpFieldLabel(field, value));
      }
      if (typeof value === "object") {
        try {
          return JSON.stringify(value, null, 2);
        } catch (err) {
          return String(value);
        }
      }
      return String(value);
    }

    function mpAssetValues(value) {
      if (Array.isArray(value)) return value.map((item) => String(item || "").trim()).filter(Boolean);
      if (typeof value === "string") {
        const text = value.trim();
        if (!text) return [];
        if (text.startsWith("[")) {
          try {
            return mpAssetValues(JSON.parse(text));
          } catch (err) {
            return [text];
          }
        }
        return text.split(/\r?\n|,/).map((item) => String(item || "").trim()).filter(Boolean);
      }
      if (value == null) return [];
      return [String(value).trim()].filter(Boolean);
    }

    function mpAssetLabel(url) {
      const clean = String(url || "").split("#")[0].split("?")[0];
      const parts = clean.split("/");
      return parts[parts.length - 1] || String(url || "");
    }

    function mpRecordCover(item) {
      if (item && item.cover) return item.cover;
      const data = (item && item.data) || {};
      for (const field of mpFields()) {
        if (!field || !field.name) continue;
        const componentType = String((((field || {}).component || {}).type || "")).toLowerCase();
        if (componentType !== "image" && componentType !== "images") continue;
        const items = mpAssetValues(data[field.name]);
        if (items.length) return items[0];
      }
      return "";
    }

    function mpOptionsSummary(options) {
      const source = options || {};
      const keys = Object.keys(source);
      if (!keys.length) return "";
      return keys.slice(0, 2).map((key) => {
        const value = source[key];
        if (typeof value === "object") {
          try {
            return `${key}: ${JSON.stringify(value)}`;
          } catch (err) {
            return `${key}: [object]`;
          }
        }
        return `${key}: ${String(value)}`;
      }).join(" | ");
    }

    function mpOptionsSummaryForMount(slotKey, mount) {
      const source = (mount && mount.options) || {};
      const keys = Object.keys(source);
      if (!keys.length) return "";
      const provider = mpResolveProviderDefinition(
        slotKey || "",
        (mount && mount.providerPlugin) || "",
        (mount && mount.serviceXid) || ""
      );
      const parts = [];
      for (const key of keys) {
        const field = mpProviderOptionField(provider, key);
        const text = field ? mpDisplayText(field, source[key]) : mpOptionsSummary({ [key]: source[key] });
        if (!text) continue;
        parts.push(field ? `${field.title || key}: ${text}` : text);
        if (parts.length >= 2) break;
      }
      return parts.join(" | ");
    }

    function mpSummary(item) {
      if (item && item.searchSnippet) return item.searchSnippet;
      const data = (item && item.data) || {};
      const fields = mpListFields();
      const parts = [];
      for (const field of fields) {
        if (!field || !field.name) continue;
        const role = String((((field || {}).semantic || {}).role) || "").toLowerCase();
        if (role === "title") continue;
        const text = mpDisplayText(field, data[field.name]);
        if (text) parts.push(`${field.title || field.name}: ${text}`);
        if (parts.length >= 2) break;
      }
      return parts.join(" | ") || ((item && item.summary) || "");
    }

    function mpChips(item) {
      const data = (item && item.data) || {};
      const chips = [];
      if (item && item.searchScore != null) chips.push(`Score: ${item.searchScore}`);
      if (item && item.tagNamesText) chips.push(`标签: ${item.tagNamesText}`);
      if (item && item.topicTitlesText) chips.push(`专题: ${item.topicTitlesText}`);
      if (mpHasAbilityPack("content.comment")) chips.push(`评论: ${item && item.visibleCommentCount ? item.visibleCommentCount : 0}`);
      if (mpHasAbilityPack("content.like")) chips.push(`点赞: ${item && item.likeCount ? item.likeCount : 0}`);
      if (mpHasAbilityPack("content.view-stat")) chips.push(`访问: ${item && item.viewCount ? item.viewCount : 0}`);
      for (const field of mpListFields()) {
        if (!field || !field.name) continue;
        const role = String((((field || {}).semantic || {}).role) || "").toLowerCase();
        if (role === "title") continue;
        const text = mpDisplayText(field, data[field.name]);
        if (text) chips.push(`${field.title || field.name}: ${text}`);
        if (chips.length >= 3) break;
      }
      return chips;
    }

    function mpHighlightText(text, term) {
      const source = String(text || "");
      const query = String(term || "").trim();
      const lowerSource = source.toLowerCase();
      const lowerQuery = query.toLowerCase();
      let pos = 0;
      let next = lowerQuery ? lowerSource.indexOf(lowerQuery) : -1;
      let html = "";
      if (!query || next < 0) return mpEscape(source);
      while (next >= 0) {
        html += mpEscape(source.slice(pos, next));
        html += `<mark>${mpEscape(source.slice(next, next + query.length))}</mark>`;
        pos = next + query.length;
        next = lowerSource.indexOf(lowerQuery, pos);
      }
      html += mpEscape(source.slice(pos));
      return html;
    }

    function mpRenderList() {
      const host = document.getElementById("mp_list");
      const templateKey = mpCategoryTemplateKey();
      host.className = "list template-" + templateKey;
      document.getElementById("mp_metric_count").innerText = String(mp.items.length);
      if (!mp.items.length) {
        host.innerHTML = "<div class='empty'>还没有已发布记录。</div>";
        return;
      }
      host.innerHTML = mp.items.map((item) => {
        const active = item.id === mp.currentId ? " active" : "";
        const chips = mpChips(item).map((text) => `<span class="chip">${mpEscape(text)}</span>`).join("");
        const coverUrl = mpRecordCover(item);
        const cover = coverUrl ? `<img class="item-cover" src="${mpEscape(coverUrl)}" alt="${mpEscape(item.title || `#${item.id}`)}" loading="lazy">` : "";
        const metaText = item.publishedAtText || item.updateTimeText || "";
        const titleHtml = mpHighlightText(item.title || `#${item.id}`, mp.query);
        const summaryHtml = mpHighlightText(mpSummary(item) || "点击查看详情。", mp.query);
        return `<article class="item${active}" onclick="mpLoadDetail(${item.id})">${cover}<strong>${titleHtml}</strong><span>${summaryHtml}</span><div class="chip-row">${chips}</div><span>${mpEscape(metaText)}</span></article>`;
      }).join("");
    }

    function mpRenderCapabilitySlots() {
      const host = document.getElementById("mp_slot_list");
      const registry = mpMountRegistry();
      const mounts = registry.mounts || [];
      const slots = mpCapabilitySlots();
      document.getElementById("mp_metric_slots").innerText = `${registry.mountedCount || 0}/${registry.declaredCount || slots.length}`;
      document.getElementById("mp_metric_slots_detail").innerText = mpMountSummaryText(registry, slots.length);
      if (!mounts.length) {
        host.innerHTML = "<div class='slot-card'><strong>还没有能力槽位</strong><span>后续能力包可以通过已声明契约挂载进来。</span></div>";
        return;
      }
      host.innerHTML = mounts.map((item) => {
        const slot = item.slot || {};
        const mount = item.mount || {};
        const key = slot.key || item.slotKey || "slot";
        const title = slot.title || slot.name || key;
        const desc = slot.desc || "为外部能力包预留的接入点。";
        const status = item.status || "unresolved";
        const provider = item.provider || mount.providerPlugin || mount.serviceXid || slot.serviceXid || "";
        const surface = ((mount.mount || {}).surface) || mount.surface || ((slot.mount || {}).surface) || "";
        const message = item.message || "";
        const publicEntry = mount.publicEntry || {};
        const scopeText = mpCurrentScopeText();
        const linkHref = mpBuildMountHref(item, publicEntry.href || "");
        const optionSummary = mpOptionsSummaryForMount(key, mount);
        const inlineAction = linkHref ? `<a href="javascript:void(0)" onclick='mpOpenWorkspace(${JSON.stringify(key)})'>${mpEscape(publicEntry.title || "内嵌打开能力")}</a>` : "";
        const link = linkHref ? `<a href="${mpEscape(linkHref)}" target="_blank" rel="noreferrer">新标签打开</a>` : "";
        return `<article class="slot-card${mpSlotStateClass(status)}"><strong>${mpEscape(title)}</strong><span>${mpEscape(key)} | ${mpEscape(mpSlotStateText(status))}</span><span>${mpEscape(provider || "尚未绑定提供方")}</span>${surface ? `<span>${mpEscape(`挂载面：${surface}`)}</span>` : ""}<span>${mpEscape(desc)}</span><span>${mpEscape(scopeText)}</span>${optionSummary ? `<span>${mpEscape(`选项：${optionSummary}`)}</span>` : ""}${message ? `<span>${mpEscape(message)}</span>` : ""}${inlineAction}${link}</article>`;
      }).join("");
    }

    function mpFieldIsFullWidth(field, value) {
      const explicitSpan = Number((field || {}).layoutSpan || 0);
      const componentType = String((((field || {}).component || {}).type || "")).toLowerCase();
      if (explicitSpan >= 2) {
        return true;
      }
      if (componentType === "textarea" || componentType === "markdown" || componentType === "richtext" || componentType === "code"
        || componentType === "image" || componentType === "images" || componentType === "file" || componentType === "files") {
        return true;
      }
      return String(mpDisplayText(field, value)).length > 80;
    }

    function mpRenderAssetFieldValue(value, isImage) {
      const items = mpAssetValues(value);
      if (!items.length) return "<div>Empty</div>";
      if (isImage) {
        return `<div class="detail-assets">${items.map((url) => `<a class="detail-asset-image" href="${mpEscape(url)}" target="_blank" rel="noopener noreferrer"><img src="${mpEscape(url)}" alt="${mpEscape(mpAssetLabel(url) || "image")}" loading="lazy"></a>`).join("")}</div>`;
      }
      return `<div class="detail-assets files">${items.map((url) => `<a class="detail-asset-file" href="${mpEscape(url)}" target="_blank" rel="noopener noreferrer"><i class="layui-icon layui-icon-link"></i><span>${mpEscape(mpAssetLabel(url) || url)}</span></a>`).join("")}</div>`;
    }

    function mpRenderFieldValue(field, value) {
      const componentType = String((((field || {}).component || {}).type || "")).toLowerCase();
      if (value == null || value === "") {
        return "<div>Empty</div>";
      }
      if (componentType === "image" || componentType === "images") {
        return mpRenderAssetFieldValue(value, true);
      }
      if (componentType === "file" || componentType === "files") {
        return mpRenderAssetFieldValue(value, false);
      }
      if (componentType === "richtext") {
        return `<div class="richtext">${String(value)}</div>`;
      }
      if (componentType === "markdown" || componentType === "code" || typeof value === "object") {
        const text = typeof value === "string" ? value : mpDisplayText(field, value);
        return `<pre>${mpEscape(text)}</pre>`;
      }
      return `<div>${mpEscape(mpDisplayText(field, value))}</div>`;
    }

    function mpNativeAbilityPanels(item) {
      const panels = [];
      if (mpHasAbilityPack("content.view-stat")) {
        panels.push(`<section class="native-panel" id="mp_view_panel"><h3>访问统计</h3><div class="native-metrics"><span><strong id="mp_view_count">0</strong>浏览</span><span><strong id="mp_unique_view_count">0</strong>访客</span></div></section>`);
      }
      if (mpHasAbilityPack("content.static")) {
        panels.push(`<section class="native-panel" id="mp_static_panel"><h3>静态化</h3><div class="native-metrics"><span id="mp_static_path">正在检查静态页面...</span><button class="native-action secondary" type="button" onclick="mpGenerateStatic()">生成静态页</button><a class="workspace-button" id="mp_static_link" href="javascript:void(0)" target="_blank" rel="noopener noreferrer" style="display:none;">打开静态页</a></div></section>`);
      }
      if (mpHasAbilityPack("content.like")) {
        panels.push(`<section class="native-panel" id="mp_like_panel"><h3>点赞</h3><div class="native-metrics"><span><strong id="mp_like_count">0</strong>点赞</span><button class="native-action" type="button" id="mp_like_button" onclick="mpToggleLike()">点赞</button></div></section>`);
      }
      if (mpHasAbilityPack("content.comment")) {
        panels.push(`<section class="native-panel" id="mp_comment_panel"><h3>评论</h3><form class="native-form" onsubmit="mpSubmitComment(event)"><input id="mp_comment_author" type="text" placeholder="昵称"><textarea id="mp_comment_body" placeholder="写下评论"></textarea><button class="native-action" type="submit">提交评论</button></form><div class="native-metrics" style="margin-top:10px;"><span><strong id="mp_comment_count">0</strong>评论</span><span id="mp_comment_status"></span></div><div class="comment-list" id="mp_comment_list"></div></section>`);
      }
      if (!panels.length) return "";
      return `<section class="native-abilities" id="mp_native_abilities">${panels.join("")}</section>`;
    }

    async function mpFetchJson(url, options) {
      const response = await fetch(url, options || { cache: "no-store" });
      const result = await response.json();
      if (!result || !result.result) {
        throw new Error((result && result.message) || "请求失败");
      }
      return result;
    }

    function mpNativePayload(contentId) {
      const key = mpClientKey("visitor");
      return {
        contentId: Number(contentId || 0),
        actorKey: key,
        cookieKey: key,
        visitorKey: key,
        referer: document.referrer || "",
        userAgent: navigator.userAgent || ""
      };
    }

    async function mpRecordView(item) {
      if (!item || !item.id || !mpHasAbilityPack("content.view-stat")) return;
      try {
        await mpFetchJson("/api/plugin/{{PLUGIN_XID}}/view/record", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(mpNativePayload(item.id))
        });
        const status = await mpFetchJson(`/api/plugin/{{PLUGIN_XID}}/view/status?contentId=${encodeURIComponent(item.id)}`, { cache: "no-store" });
        const viewEl = document.getElementById("mp_view_count");
        const uniqueEl = document.getElementById("mp_unique_view_count");
        if (viewEl) viewEl.innerText = String(status.viewCount || 0);
        if (uniqueEl) uniqueEl.innerText = String(status.uniqueViewCount || 0);
        await mpLoadViewRank();
      } catch (err) {
        const panel = document.getElementById("mp_view_panel");
        if (panel) panel.style.display = "none";
      }
    }

    function mpRankTitle(row) {
      const id = Number((row || {}).contentId || 0);
      const item = (mp.items || []).find((entry) => Number(entry.id || 0) === id);
      return (item && item.title) || `内容 #${id}`;
    }

    function mpRenderViewRank() {
      const panel = document.getElementById("mp_rank_panel");
      const host = document.getElementById("mp_rank_list");
      if (!panel || !host) return;
      if (!mpHasAbilityPack("content.view-stat")) {
        panel.style.display = "none";
        return;
      }
      panel.style.display = "";
      const rows = Array.isArray(mp.ranks) ? mp.ranks : [];
      host.innerHTML = rows.length ? rows.map((row, index) => {
        const id = Number(row.contentId || 0);
        return `<button class="rank-item" type="button" onclick="mpLoadDetail(${id})"><strong>${index + 1}. ${mpEscape(mpRankTitle(row))}</strong><span>${mpEscape(row.viewCount || 0)} 浏览 / ${mpEscape(row.uniqueViewCount || 0)} 访客</span></button>`;
      }).join("") : "<div class='empty'>暂无排行。</div>";
    }

    async function mpLoadViewRank() {
      if (!mpHasAbilityPack("content.view-stat")) {
        mp.ranks = [];
        mpRenderViewRank();
        return;
      }
      try {
        const result = await mpFetchJson("/api/plugin/{{PLUGIN_XID}}/view/rank", { cache: "no-store" });
        mp.ranks = result && result.result ? (result.data || []) : [];
      } catch (err) {
        mp.ranks = [];
      }
      mpRenderViewRank();
    }

    async function mpLoadLike(item) {
      if (!item || !item.id || !mpHasAbilityPack("content.like")) return;
      try {
        const key = encodeURIComponent(mpClientKey("visitor"));
        const result = await mpFetchJson(`/api/plugin/{{PLUGIN_XID}}/like/status?contentId=${encodeURIComponent(item.id)}&actorKey=${key}&cookieKey=${key}`, { cache: "no-store" });
        const countEl = document.getElementById("mp_like_count");
        const button = document.getElementById("mp_like_button");
        mp.abilityState.like = { liked: !!result.liked };
        if (countEl) countEl.innerText = String(result.likeCount || 0);
        if (button) {
          button.innerText = result.liked ? "取消点赞" : "点赞";
          button.className = result.liked ? "native-action secondary" : "native-action";
        }
      } catch (err) {
        const panel = document.getElementById("mp_like_panel");
        if (panel) panel.style.display = "none";
      }
    }

    function mpSetStaticArtifact(path) {
      const pathEl = document.getElementById("mp_static_path");
      const linkEl = document.getElementById("mp_static_link");
      const text = String(path || "").trim();
      if (pathEl) pathEl.innerText = text ? `静态路径：${text}` : "尚未生成静态页。";
      if (linkEl) {
        if (text) {
          linkEl.href = text.charAt(0) === "/" ? text : `/${text}`;
          linkEl.style.display = "";
        } else {
          linkEl.removeAttribute("href");
          linkEl.style.display = "none";
        }
      }
    }

    async function mpLoadStatic(item) {
      if (!item || !item.id || !mpHasAbilityPack("content.static")) return;
      try {
        const result = await mpFetchJson(`/api/plugin/{{PLUGIN_XID}}/static/preview?targetId=${encodeURIComponent(item.id)}`, { cache: "no-store" });
        const artifact = result.data || {};
        mpSetStaticArtifact(artifact.path || "");
      } catch (err) {
        const panel = document.getElementById("mp_static_panel");
        if (panel) panel.style.display = "none";
      }
    }

    async function mpGenerateStatic() {
      const item = mp.currentItem;
      if (!item || !item.id) return;
      try {
        const result = await mpFetchJson("/api/plugin/{{PLUGIN_XID}}/static/generate", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ targetId: Number(item.id) })
        });
        mpSetStaticArtifact(result.path || "");
        mpSetStatus("mp_detail_status", "静态页已生成。");
      } catch (err) {
        mpSetStatus("mp_detail_status", err && err.message ? err.message : String(err), "#f87171");
      }
    }

    async function mpToggleLike() {
      const item = mp.currentItem;
      if (!item || !item.id) return;
      const liked = !!(((mp.abilityState || {}).like || {}).liked);
      const url = liked ? "/api/plugin/{{PLUGIN_XID}}/like/cancel" : "/api/plugin/{{PLUGIN_XID}}/like/create";
      try {
        await mpFetchJson(url, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(mpNativePayload(item.id))
        });
        await mpLoadLike(item);
      } catch (err) {
        mpSetStatus("mp_detail_status", err && err.message ? err.message : String(err), "#f87171");
      }
    }

    async function mpLoadComments(item) {
      if (!item || !item.id || !mpHasAbilityPack("content.comment")) return;
      try {
        const count = await mpFetchJson(`/api/plugin/{{PLUGIN_XID}}/comment/count?contentId=${encodeURIComponent(item.id)}`, { cache: "no-store" });
        const list = await mpFetchJson(`/api/plugin/{{PLUGIN_XID}}/comment/list?contentId=${encodeURIComponent(item.id)}`, { cache: "no-store" });
        const countEl = document.getElementById("mp_comment_count");
        const host = document.getElementById("mp_comment_list");
        if (countEl) countEl.innerText = String(count.visibleCount || 0);
        if (host) {
          const comments = list.data || [];
          host.innerHTML = comments.length ? comments.map((comment) => `<article class="comment-item"><strong>${mpEscape(comment.authorName || "访客")}</strong><span>${mpEscape(comment.createTimeText || "")}</span><p>${mpEscape(comment.body || "")}</p></article>`).join("") : "<div class='empty'>暂无评论。</div>";
        }
      } catch (err) {
        const panel = document.getElementById("mp_comment_panel");
        if (panel) panel.style.display = "none";
      }
    }

    async function mpSubmitComment(event) {
      if (event) event.preventDefault();
      const item = mp.currentItem;
      const authorEl = document.getElementById("mp_comment_author");
      const bodyEl = document.getElementById("mp_comment_body");
      const statusEl = document.getElementById("mp_comment_status");
      const body = String((bodyEl || {}).value || "").trim();
      if (!item || !item.id || !body) {
        if (statusEl) statusEl.innerText = "请输入评论内容。";
        return;
      }
      try {
        const result = await mpFetchJson("/api/plugin/{{PLUGIN_XID}}/comment/create", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            contentId: Number(item.id),
            authorName: String((authorEl || {}).value || "").trim() || "访客",
            body: body
          })
        });
        if (bodyEl) bodyEl.value = "";
        if (statusEl) statusEl.innerText = Number(result.status || 0) === 1 ? "评论已发布。" : "评论已提交，等待审核。";
        await mpLoadComments(item);
      } catch (err) {
        if (statusEl) statusEl.innerText = err && err.message ? err.message : String(err);
      }
    }

    async function mpLoadNativeAbilities(item) {
      mp.abilityState = {};
      await Promise.all([mpRecordView(item), mpLoadStatic(item), mpLoadLike(item), mpLoadComments(item)]);
    }

    function mpRenderDetail(item) {
      const host = document.getElementById("mp_detail");
      mp.currentItem = item || null;
      mpRenderCapabilitySlots();
      mpSyncWorkspaceForContext();
      if (!item) {
        host.innerHTML = "<div class='empty'>请选择一条记录查看详情。</div>";
        mpUpdateHead(null);
        return;
      }
      const fields = mpDetailFields();
      const cards = fields.map((field) => {
        const value = item.data ? item.data[field.name] : "";
        const full = mpFieldIsFullWidth(field, value) ? " full" : "";
        return `<section class="detail-card${full}"><span>${mpEscape(field.title || field.name)}</span>${mpRenderFieldValue(field, value)}</section>`;
      }).join("");
      const chips = mpChips(item).map((text) => `<span class="chip">${mpEscape(text)}</span>`).join("");
      const coverUrl = mpRecordCover(item);
      const cover = coverUrl ? `<img class="detail-cover" src="${mpEscape(coverUrl)}" alt="${mpEscape(item.title || `#${item.id}`)}" loading="lazy">` : "";
      const metaParts = [];
      if (mpDetailShowPublishedAt() && item.publishedAtText) metaParts.push(`发布于 ${item.publishedAtText}`);
      if (mpDetailShowAuthor()) {
        const authorText = mpDetailAuthorText(item);
        if (authorText) metaParts.push(`作者 ${authorText}`);
      }
      if (item.updateTimeText) metaParts.push(`更新于 ${item.updateTimeText}`);
      host.innerHTML = `${cover}<h2 class="detail-title">${mpEscape(item.title || `#${item.id}`)}</h2><div class="detail-meta">${mpEscape(metaParts.join(" | ") || `更新于 ${item.updateTimeText || ""}`)}</div><div class="chip-row">${chips}</div><div class="detail-grid">${cards || "<div class='detail-card full'><span>内容</span><div>还没有配置详情字段。</div></div>"}</div>${mpNativeAbilityPanels(item)}<section class="workspace-shell"><div class="workspace-toolbar"><div><div class="workspace-title" id="mp_workspace_title">尚未打开能力面板</div><div class="detail-meta" id="mp_workspace_scope" style="margin:6px 0 0;">选择一个已挂载槽位后即可在这里打开前台能力。</div></div><div class="toolbar"><button class="workspace-button" type="button" onclick="mpRefreshWorkspace()">刷新工作区</button><button class="workspace-button" type="button" onclick="mpOpenWorkspaceNewTab()">新标签打开</button><button class="workspace-button danger" type="button" onclick="mpCloseWorkspace()">关闭工作区</button></div></div><div id="mp_workspace_host"><div class="workspace-empty">选择一个已挂载槽位后即可在这里打开前台能力。</div></div></section>`;
      mpLoadNativeAbilities(item).catch((err) => mpSetStatus("mp_detail_status", err && err.message ? err.message : String(err), "#f87171"));
      mpRenderWorkspace();
    }

    async function mpLoadMeta() {
      const response = await fetch("/api/plugin/{{PLUGIN_XID}}/meta", { cache: "no-store" });
      const result = await response.json();
      if (!result || !result.result) throw new Error((result && result.message) || "元数据加载失败");
      mp.meta = result.data || {};
      const spec = (mp.meta || {}).spec || {};
      const identity = spec.identity || {};
      document.getElementById("mp_title").innerText = mp.meta.title || "{{PLUGIN_XID}}";
      document.getElementById("mp_desc").innerText = identity.description || "由内容模型驱动的展示页面。";
      document.getElementById("mp_metric_plugin").innerText = (mp.meta && mp.meta.pluginXid) || "{{PLUGIN_XID}}";
      document.getElementById("mp_metric_fields").innerText = String(mpDetailFields().length);
      await mpLoadTaxonomyOptions();
      mpRenderHeroMetadata();
      mpRenderListControls();
      mpRenderCapabilitySlots();
      mpRenderWorkspace();
      mpUpdateHead(null);
    }

    async function mpLoadTaxonomyOptions() {
      const tasks = [];
      if (mpHasAbilityPack("content.category")) {
        tasks.push(fetch("/api/plugin/{{PLUGIN_XID}}/category/list", { cache: "no-store" })
          .then((response) => response.json())
          .then((result) => { mp.categories = result && result.result ? (result.data || []) : []; })
          .catch(() => { mp.categories = []; }));
      } else {
        mp.categories = [];
      }
      if (mpHasAbilityPack("content.tag")) {
        tasks.push(fetch("/api/plugin/{{PLUGIN_XID}}/tag/list", { cache: "no-store" })
          .then((response) => response.json())
          .then((result) => { mp.tags = result && result.result ? (result.data || []) : []; })
          .catch(() => { mp.tags = []; }));
      } else {
        mp.tags = [];
      }
      if (mpHasAbilityPack("content.topic")) {
        tasks.push(fetch("/api/plugin/{{PLUGIN_XID}}/topic/list", { cache: "no-store" })
          .then((response) => response.json())
          .then((result) => { mp.topics = result && result.result ? (result.data || []) : []; })
          .catch(() => { mp.topics = []; }));
      } else {
        mp.topics = [];
      }
      await Promise.all(tasks);
    }

    async function mpLoadList() {
      const params = new URLSearchParams();
      params.set("page", "1");
      params.set("limit", String(mpListPageSize()));
      if (mp.query) params.set("q", mp.query);
      if (mp.sortBy) params.set("sortBy", mp.sortBy);
      if (mp.sortDir) params.set("sortDir", mp.sortDir);
      if (mp.categoryId) params.set("categoryId", mp.categoryId);
      if (mp.tagId) params.set("tagId", mp.tagId);
      if (mp.topicId) params.set("topicId", mp.topicId);
      for (const field of mpFilterableFields()) {
        const value = String(((mp.fieldFilters || {})[field.name] || "")).trim();
        if (value !== "") {
          params.set(mpFieldQueryKey(field), value);
        }
      }
      const hasExtraFilters = !!(mp.categoryId || mp.tagId || mp.topicId || Object.keys(mp.fieldFilters || {}).length);
      const listApi = (mp.query && mpHasAbilityPack("content.search") && !hasExtraFilters) ? "search" : "list";
      const response = await fetch(`/api/plugin/{{PLUGIN_XID}}/${listApi}?${params.toString()}`, { cache: "no-store" });
      const result = await response.json();
      if (!result || !result.result) throw new Error((result && result.message) || "列表加载失败");
      mp.items = result.data || [];
      mp.listMeta = {
        scanLimit: Number(result.scanLimit || 0),
        scanLimitReached: !!result.scanLimitReached
      };
      mpRenderList();
      mpRenderViewRank();
      if (mp.listMeta.scanLimitReached) {
        mpSetStatus("mp_nav_status", `${mp.items.length} rows loaded; scan limit ${mp.listMeta.scanLimit} reached.`, "#fbbf24");
        return;
      }
      mpSetStatus("mp_nav_status", `${mp.items.length} 条已发布记录已加载${mp.query ? `，关键词“${mp.query}”` : ""}。`);
    }

    async function mpLoadDetail(id, slug) {
      try {
        mp.currentId = Number(id || 0);
        mp.currentItem = null;
        mpRenderList();
        mpRenderCapabilitySlots();
        mpSyncWorkspaceForContext();
        const query = slug
          ? `slug=${encodeURIComponent(slug)}`
          : `id=${encodeURIComponent(id)}`;
        const response = await fetch(`/api/plugin/{{PLUGIN_XID}}/detail?${query}`, { cache: "no-store" });
        const result = await response.json();
        if (!result || !result.result) throw new Error((result && result.message) || "详情加载失败");
        mpRenderDetail(result.data || null);
        const loaded = result.data || {};
        mp.currentId = Number(loaded.id || mp.currentId || 0);
        mpRenderList();
        mpSyncUrl(loaded && loaded.id ? loaded : null);
        const seoMeta = loaded && loaded.id ? await mpLoadSeoMeta(loaded) : null;
        mpUpdateHead(loaded && loaded.id ? loaded : null, seoMeta);
        mpSetStatus("mp_detail_status", slug ? `已加载记录 ${slug}。` : `已加载记录 #${id}。`);
      } catch (err) {
        mpSetStatus("mp_detail_status", err && err.message ? err.message : String(err), "#f87171");
      }
    }

    async function mpBoot() {
      try {
        await mpLoadMeta();
        mpApplyUrlState();
        const params = new URLSearchParams(window.location.search);
        mpRenderListControls();
        await mpLoadList();
        await mpLoadViewRank();
        const requestedId = Number(params.get("id") || 0);
        const requestedSlug = String(params.get("slug") || mpPrettySlugFromPath() || "");
        const slugItem = requestedSlug
          ? (mp.items.find((item) => String(item.slug || "") === requestedSlug) || null)
          : null;
        const initialId = (requestedId > 0 && mp.items.some((item) => item.id === requestedId))
          ? requestedId
          : (mp.items[0] ? mp.items[0].id : 0);
        if (requestedSlug) {
          await mpLoadDetail(slugItem ? slugItem.id : 0, requestedSlug);
        } else if (initialId > 0) {
          await mpLoadDetail(initialId);
        } else {
          mpRenderDetail(null);
          mpSyncUrl(null);
        }
      } catch (err) {
        mpHandleError(err);
      }
    }

    window.addEventListener("unhandledrejection", function(event) {
      mpHandleError(event.reason);
      event.preventDefault();
    });

    mpBoot();
  </script>
</body>
</html>
