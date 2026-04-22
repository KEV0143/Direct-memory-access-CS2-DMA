<div align="center">

<h1>{ ... } Direct Memory Access — CS2 DMA</h1>

<p style="max-width: 800px;">
An open-source software framework designed for the research and implementation of Direct Memory Access (DMA) interaction with Counter-Strike 2. The system demonstrates established methodologies for memory reading, game entity parsing, network synchronization, and data visualization. It serves as a practical platform for studying low-level programming, PCIe architecture, and reverse engineering techniques.
</p>

<p style="color: #ff6b6b; font-weight: 600; font-size: 0.95em; margin: 10px 0;">
{ ! } This project is for educational and research purposes only <br>
{ ! } This is an external method and requires a 
<a href="https://github.com/ufrisk/pcileech-fpga" target="_blank">DMA Card</a> to work.
</p>

<p>
<a href="https://github.com/KEV0143/Direct-memory-access-CS2-DMA/releases/latest"><img src="https://img.shields.io/github/v/release/KEV0143/Direct-memory-access-CS2-DMA?style=flat-square&color=blue" alt="Latest Release"></a>
&nbsp;&nbsp;
<a href="https://github.com/KEV0143/Direct-memory-access-CS2-DMA/releases"><img src="https://img.shields.io/github/downloads/KEV0143/Direct-memory-access-CS2-DMA/total?style=flat-square&color=success" alt="Total Downloads"></a>
&nbsp;&nbsp;
<a href="https://github.com/KEV0143/Direct-memory-access-CS2-DMA/releases"><img src="https://img.shields.io/badge/Platform-Windows-lightgrey?style=flat-square" alt="Supported Platforms"></a>
&nbsp;&nbsp;
<a href="https://github.com/KEV0143/Direct-memory-access-CS2-DMA/blob/main/LICENSE"><img src="https://img.shields.io/badge/License-Apache_2.0-blue?style=flat-square" alt="License"></a>
<br>
<a href="https://github.com/KEV0143/Direct-memory-access-CS2-DMA/pulse"><img src="https://img.shields.io/github/release-date/KEV0143/Direct-memory-access-CS2-DMA?style=flat-square" alt="Release Date"></a>
&nbsp;&nbsp;
<img src="https://img.shields.io/badge/Language-C%2B%2B-blue?style=flat-square" alt="Language">
&nbsp;&nbsp;
<a href="https://github.com/KEV0143/Direct-memory-access-CS2-DMA/stargazers"><img src="https://img.shields.io/github/stars/KEV0143/Direct-memory-access-CS2-DMA?style=flat-square&color=yellow" alt="Stars"></a>
</p>

<img height="125" alt="Logo" src="https://github.com/user-attachments/assets/6edda043-63f4-44c1-8a1f-25f7d8b7bf5c" />

<br>
<br>

<p style="font-size: 1.05em; color: #7f8c8d; margin: 20px 0;">
<b>Are you a developer?</b> Suggest improvements to the project!<br>
<b>Telegram:</b> <a href="https://t.me/ne_sravnim" style="text-decoration: none; color: #0088cc;">@ne_sravnim</a> &nbsp;|&nbsp; 💬 <b>Discord:</b> CoraKevq
</p>

<p style="font-size: 1.05em; color: #7f8c8d; margin: 15px 0;">
💙 <b>Want to support this research?</b> Feel free to reach out via Telegram!
</p>

<br>

<p style="font-size: 1.1em;">
  &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
  <a href="#" style="text-decoration: none;">
    <b>Русская локализация</b>
  </a>
  &nbsp;&nbsp;&nbsp;&nbsp;|&nbsp;&nbsp;&nbsp;&nbsp;
  <a href="https://github.com/KEV0143/Direct-memory-access-CS2-DMA/blob/main/README.md" style="text-decoration: none;">
    <b>English Localization</b>
  </a>
  &nbsp;&nbsp;&nbsp;&nbsp;|&nbsp;&nbsp;&nbsp;&nbsp;
  <a href="#" style="text-decoration: none;">
    <b>中文本地化</b>
  </a>
</p>

<h2>WebRadar</h2>
<img width="1920" height="1080" alt="WebRadar" src="https://github.com/user-attachments/assets/13d3c864-22f5-4745-99db-338cb28c5ecc" style="border-radius: 8px;" />
<p><code> How WebRadar works: DMA reads data from CS2’s memory, sends it to a local backend, and 
 the backend streams player positions to the browser via WebSocket for map display. 
 The address in the browser points to the local web service, for example 192.168.x.x or localhost or 0.0.0.0 
 Sequence: DMA → memory reading → backend → WebSocket → map display. </code></p>

<h2>ESP</h2>

| **Display** | **Dichen Fuser V6** |
| :---: | :---: |
| <img src="https://github.com/user-attachments/assets/d80a31e9-3797-48da-9af3-78aa48e27f3f" width="380"/> | <img src="https://github.com/user-attachments/assets/c52be22c-2df2-44e5-9e22-6885467b78b0" width="380"/> |
| <img src="https://github.com/user-attachments/assets/27424760-5d6f-4f81-8d19-66b38f9196d5" width="380"/> | <img src="https://github.com/user-attachments/assets/1aac33bc-ad1c-4e0d-8b47-3d351513fc66" width="380"/> |
<p><code>How ESP works: The game and the DMA Card are connected to the main PC, while the software runs on a second PC. The visual output is shown by overlaying the second display onto the main monitor using Dichen Fuser v6.
Sequence: main PC → DMA data → second PC software → display overlay via Dichen Fuser v6.</code></p>

<h2>Menu UI</h2>
<table align="center" cellpadding="6" cellspacing="0">
  <tr>
    <td align="center" valign="top" width="390">
      <b>Radar UI</b><br>
      <img src="https://github.com/user-attachments/assets/f5a3ee48-d392-424f-a2bb-0642a5acbaaa" height="300"/>
    </td>
    <td align="center" valign="top" width="390">
      <b>WebRadar UI</b><br>
      <img src="https://github.com/user-attachments/assets/47b94f3d-2976-4f96-84d7-7df5f4fce9af" height="300"/>
    </td>
  </tr>
  <tr>
    <td align="center" valign="top" width="390">
      <b>Settings / Debug UI</b><br>
      <img src="https://github.com/user-attachments/assets/af59f7a1-8f2e-4875-804d-2fd577f263ef" height="190"/>
    </td>
    <td align="center" valign="top" width="390">
      <b>Main Start UI</b><br>
      <img src="https://github.com/user-attachments/assets/d00d9a54-3e93-4fcb-9475-9d7e680599a7" height="190"/>
    </td>
  </tr>
  <tr>
    <td align="center" colspan="2">
      <b>ESP UI</b><br>
      <img src="https://github.com/user-attachments/assets/d21e3351-e655-488c-903c-133879c4f45e" width="790"/>
    </td>
  </tr>
</table>
<p>
<code>Radar UI</code>
<code>The main radar workspace for monitoring live in-session information. It displays tracked data, current status, and radar-related controls in a single overview panel.</code>

<code>WebRadar UI</code>
<code>The web-based radar access panel. It provides a browser-friendly entry point for the radar system, including connection setup, quick-access options, and session information.</code>

<code>Settings / Debug UI</code>
<code>The configuration and diagnostics panel used for advanced adjustment and testing. It allows fine-tuning of behavior, debugging workflows, and verification of internal states during runtime.</code>

<code>Main Start UI</code>
<code>The startup and initialization window that logs the main launch sequence. It shows connection state, version checks, offset loading, subsystem initialization, and readiness status before the main workflow begins.</code>

<code>ESP UI</code>
<code>The main ESP configuration interface for visual customization and preview. It is used to control rendering options, enable or disable individual elements, preview layouts, and adjust colors for different ESP components.</code>
</p>

</div>
