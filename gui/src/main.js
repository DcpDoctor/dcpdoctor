import { invoke } from "@tauri-apps/api/core";

// ── DOM Elements ──────────────────────────────────
const dropZone = document.getElementById("drop-zone");
const folderInput = document.getElementById("folder-input");
const dcpPath = document.getElementById("dcp-path");
const btnValidate = document.getElementById("btn-validate");
const progressSection = document.getElementById("progress-section");
const progressFill = document.getElementById("progress-fill");
const progressText = document.getElementById("progress-text");
const resultsSection = document.getElementById("results-section");
const summaryCard = document.getElementById("summary-card");
const summaryIcon = document.getElementById("summary-icon");
const summaryTitle = document.getElementById("summary-title");
const summaryDetail = document.getElementById("summary-detail");
const statErrors = document.getElementById("stat-errors");
const statWarnings = document.getElementById("stat-warnings");
const statInfo = document.getElementById("stat-info");
const resultsBody = document.getElementById("results-body");
const noResults = document.getElementById("no-results");
const versionLabel = document.getElementById("version-label");

// ── Version ───────────────────────────────────────
async function loadVersion() {
  try {
    const ver = await invoke("get_version");
    versionLabel.textContent = ver;
  } catch {
    versionLabel.textContent = "";
  }
}
loadVersion();

// ── Drop Zone ─────────────────────────────────────
dropZone.addEventListener("click", () => folderInput.click());

dropZone.addEventListener("dragover", (e) => {
  e.preventDefault();
  dropZone.classList.add("drag-over");
});

dropZone.addEventListener("dragleave", () => {
  dropZone.classList.remove("drag-over");
});

dropZone.addEventListener("drop", (e) => {
  e.preventDefault();
  dropZone.classList.remove("drag-over");
  const items = e.dataTransfer?.items;
  if (items && items.length > 0) {
    // In Tauri, we get file paths from the drop
    const files = e.dataTransfer.files;
    if (files.length > 0) {
      // Use the path of the first dropped item
      dcpPath.value = files[0].path || files[0].name;
    }
  }
});

folderInput.addEventListener("change", (e) => {
  const files = e.target.files;
  if (files && files.length > 0) {
    // Extract directory path from first file
    const path = files[0].webkitRelativePath || files[0].name;
    const dir = path.split("/")[0];
    dcpPath.value = dir;
  }
});

// ── Validation ────────────────────────────────────
btnValidate.addEventListener("click", () => runValidation());
dcpPath.addEventListener("keydown", (e) => {
  if (e.key === "Enter") runValidation();
});

function getSelectedFlags() {
  const checkboxes = document.querySelectorAll('.option-chip input[type="checkbox"]:checked');
  return Array.from(checkboxes).map((cb) => cb.value);
}

async function runValidation() {
  const path = dcpPath.value.trim();
  if (!path) {
    dcpPath.focus();
    dcpPath.style.borderColor = "var(--red)";
    setTimeout(() => (dcpPath.style.borderColor = ""), 1500);
    return;
  }

  const flags = getSelectedFlags();

  // Show progress
  resultsSection.classList.add("hidden");
  progressSection.classList.remove("hidden");
  btnValidate.disabled = true;
  progressFill.style.width = "30%";
  progressText.textContent = "Validating DCP...";

  try {
    progressFill.style.width = "60%";
    progressText.textContent = "Running checks...";

    const response = await invoke("validate_dcp", { path, flags });

    progressFill.style.width = "100%";
    progressText.textContent = "Complete";
    await sleep(300);

    showResults(response);
  } catch (err) {
    progressSection.classList.add("hidden");
    alert("Validation failed: " + (err.message || err));
  } finally {
    btnValidate.disabled = false;
  }
}

function showResults(response) {
  progressSection.classList.add("hidden");
  resultsSection.classList.remove("hidden");

  const { results, summary, exit_code } = response;

  const errors = results.filter((r) => r.severity === "error").length;
  const warnings = results.filter((r) => r.severity === "warning").length;
  const infos = results.filter((r) => r.severity === "info").length;

  // Summary card
  summaryCard.className = "summary-card";
  if (errors > 0) {
    summaryCard.classList.add("invalid");
    summaryIcon.textContent = "✗";
    summaryTitle.textContent = "Validation Failed";
  } else if (warnings > 0) {
    summaryCard.classList.add("warnings-only");
    summaryIcon.textContent = "⚠";
    summaryTitle.textContent = "Passed with Warnings";
  } else {
    summaryCard.classList.add("valid");
    summaryIcon.textContent = "✓";
    summaryTitle.textContent = "DCP is Valid";
  }
  summaryDetail.textContent = summary;

  // Stats
  statErrors.textContent = errors;
  statWarnings.textContent = warnings;
  statInfo.textContent = infos;

  // Table
  resultsBody.innerHTML = "";
  if (results.length === 0) {
    noResults.classList.remove("hidden");
    document.querySelector(".results-table").classList.add("hidden");
  } else {
    noResults.classList.add("hidden");
    document.querySelector(".results-table").classList.remove("hidden");

    results.forEach((r) => {
      const tr = document.createElement("tr");
      tr.dataset.severity = r.severity;

      const badgeClass =
        r.severity === "error"
          ? "badge-error"
          : r.severity === "warning"
            ? "badge-warning"
            : "badge-info";

      tr.innerHTML = `
        <td><span class="badge ${badgeClass}">${escapeHtml(r.severity)}</span></td>
        <td><span class="code-tag">${escapeHtml(r.code)}</span></td>
        <td>${escapeHtml(r.message)}</td>
        <td><span class="file-path" title="${escapeHtml(r.file)}">${escapeHtml(shortPath(r.file))}</span></td>
      `;
      resultsBody.appendChild(tr);
    });
  }
}

// ── Filter Tabs ───────────────────────────────────
document.querySelectorAll(".filter-tab").forEach((tab) => {
  tab.addEventListener("click", () => {
    document.querySelectorAll(".filter-tab").forEach((t) => t.classList.remove("active"));
    tab.classList.add("active");

    const filter = tab.dataset.filter;
    document.querySelectorAll("#results-body tr").forEach((tr) => {
      if (filter === "all" || tr.dataset.severity === filter) {
        tr.classList.remove("hidden");
      } else {
        tr.classList.add("hidden");
      }
    });
  });
});

// ── Helpers ───────────────────────────────────────
function escapeHtml(str) {
  const div = document.createElement("div");
  div.textContent = str;
  return div.innerHTML;
}

function shortPath(path) {
  if (!path) return "";
  const parts = path.replace(/\\/g, "/").split("/");
  return parts.length > 2 ? "..." + parts.slice(-2).join("/") : path;
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

// ── Demo Results (dev mode without Tauri backend) ─
function getDemoResults() {
  return {
    results: [
      { severity: "error", code: "cpl_hash_mismatch", message: "CPL hash does not match PKL entry", file: "CPL_abc123.xml" },
      { severity: "error", code: "mxf_incomplete", message: "MXF frame count does not match CPL duration", file: "j2c_track01.mxf" },
      { severity: "warning", code: "annotation_missing", message: "AnnotationText is empty in CPL", file: "CPL_abc123.xml" },
      { severity: "warning", code: "loudness_exceeds", message: "Dialogue loudness exceeds -24 LKFS target (-21.3 LKFS)", file: "pcm_track01.mxf" },
      { severity: "info", code: "resolution_2k", message: "Detected 2K DCI resolution (2048x1080)", file: "j2c_track01.mxf" },
      { severity: "info", code: "color_xyz", message: "Color space: XYZ (CIE 1931)", file: "j2c_track01.mxf" },
    ],
    summary: "2 error(s), 2 warning(s) found.",
    exit_code: 1,
  };
}
