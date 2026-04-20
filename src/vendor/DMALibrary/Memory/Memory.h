#pragma once
#include "../pch.h"
#include "InputManager.h"
#include "Registry.h"
#include "Shellcode.h"
#include "../nt/structs.h"
#include <atomic>

class Memory
{
private:
	struct DmaInitStats
	{
		bool usedMemMap = false;
		bool reusedMemMapCache = false;
		uint64_t runtimeLibsMs = 0;
		uint64_t memMapMs = 0;
		uint64_t vmmInitMs = 0;
		uint64_t fpgaConfigMs = 0;
		uint64_t totalMs = 0;
	};

	struct LibModules
	{
		HMODULE VMM = nullptr;
		HMODULE FTD3XX = nullptr;
		HMODULE LEECHCORE = nullptr;
	};

	static inline LibModules modules { };

	struct CurrentProcessInformation
	{
		int PID = 0;
		size_t base_address = 0;
		size_t base_size = 0;
		std::string process_name = "";
	};

	static inline CurrentProcessInformation current_process { };

	static inline BOOLEAN DMA_INITIALIZED = FALSE;
	static inline BOOLEAN PROCESS_INITIALIZED = FALSE;
	static inline std::atomic<bool> SCATTER_WARN_SUPPRESSED = false;
	static inline thread_local uint32_t SCATTER_PENDING_COUNT = 0;
	static inline thread_local uint32_t SCATTER_PREPARE_FAIL_COUNT = 0;
	static inline DmaInitStats LAST_DMA_INIT_STATS = {};
	/**
	*Dumps the systems Current physical memory pages
	*To a file so we can use it in our DMA (:
	*This file it created to %temp% folder
	*@return true if successful, false if not.
	*/
	bool DumpMemoryMap(const std::string& outputPath, bool debug = false);
	bool EnsureRuntimeLibrariesLoaded();

	/**
	* brief Removes basic information related to the FPGA device
	* This is required before any DMA operations can be done.
	* To ensure the optimal safety in game cheating.
	* @return true if successful, false if not.
	*/
	bool SetFPGA();

	//shared pointer
	std::shared_ptr<c_keys> key;
	c_registry registry;
	c_shellcode shellcode;


public:
	/**
	 * brief Constructor takes a wide string of the process.
	 * Expects that all the libraries are in the root dir
	 */
	Memory();
	~Memory();

	/**
	* @brief Gets the registry object
	* @return registry class
	*/
	c_registry GetRegistry() { return registry; }

	/**
	* @brief Gets the key object
	* @return key class
	*/
	c_keys* GetKeyboard() { return key.get(); }

	/**
	* @brief Gets the shellcode object
	* @return shellcode class
	*/
	c_shellcode GetShellcode() { return shellcode; }

	/**
	* brief Initializes the DMA
	* This is required before any DMA operations can be done.
	* @param process_name the name of the process
	* @param memMap if true, will dump the memory map to a file	& make the DMA use it.
	* @return true if successful, false if not.
	*/
	bool Init(std::string process_name, bool memMap = true, bool debug = false);

	/**
	* @brief Initializes only DMA transport (without process attach).
	* @param memMap if true, uses physical memory map file.
	* @param debug enables verbose VMM output.
	* @return true if successful.
	*/
	bool InitDma(bool memMap = true, bool debug = false);
	DmaInitStats GetLastDmaInitStats() const { return LAST_DMA_INIT_STATS; }

	/**
	* @brief Attaches to a target process after DMA init.
	* @param process_name target executable name.
	* @param applyCr3Fix if true, attempts CR3 fix before module queries.
	* @return true if successful.
	*/
	bool AttachToProcess(const std::string& process_name, bool applyCr3Fix = true);
	void ResetProcessState();
	void CloseDma();

	/*This part here is things related to the process information such as Base daddy, Size ect.*/

	/**
	* brief Gets the process id of the process
	* @param process_name the name of the process
	* @return the process id of the process
	*/
	DWORD GetPidFromName(const std::string& process_name);

	/**
	* brief Gets all the processes id(s) of the process
	* @param process_name the name of the process
	* @returns all the processes id(s) of the process
	*/
	std::vector<int> GetPidListFromName(const std::string& process_name);

	/**
	* \brief Gets the module list of the process
	* \param process_name the name of the process 
	* \return all the module names of the process 
	*/
	std::vector<std::string> GetModuleList(const std::string& process_name);

	/**
	* \brief Gets the process information
	* \return the process information
	*/
	VMMDLL_PROCESS_INFORMATION GetProcessInformation();

	/**
	* \brief Gets the process peb
	* \return the process peb 
	*/
	PEB GetProcessPeb();

	/**
	* brief Gets the base address of the process
	* @param module_name the name of the module
	* @return the base address of the process
	*/
	size_t GetBaseDaddy(std::string module_name);

	/**
	* brief Gets the base size of the process
	* @param module_name the name of the module
	* @return the base size of the process
	*/
	size_t GetBaseSize(std::string module_name);

	/**
	* brief Gets the export table address of the process
	* @param import the name of the export
	* @param process the name of the process
	* @param module the name of the module that you wanna find the export in
	* @return the export table address of the export
	*/
	uintptr_t GetExportTableAddress(std::string import, std::string process, std::string module);

	/**
	* brief Gets the import table address of the process
	* @param import the name of the import
	* @param process the name of the process
	* @param module the name of the module that you wanna find the import in
	* @return the import table address of the import
	*/
	uintptr_t GetImportTableAddress(std::string import, std::string process, std::string module);

	/**
	 * \brief This fixes the CR3 fuckery that EAC does.
	 * It fixes it by iterating over all DTB's that exist within your system and looks for specific ones
	 * that nolonger have a PID assigned to them, aka their pid is 0
	 * it then puts it in a vector to later try each possible DTB to find the DTB of the process.
	 * NOTE: Using FixCR3 requires you to have symsrv.dll, dbghelp.dll and info.db
	 */
	bool FixCr3();

	/**
	 * \brief Dumps the process memory at address (requires to be a valid PE Header) to the path
	 * \param address the address to the PE Header(BaseAddress)
	 * \param path the path where you wanna save dump to
	 */
	bool DumpMemory(uintptr_t address, std::string path);

	/*This part is where all memory operations are done, such as read, write.*/

	/**
	 * \brief Scans the process for the signature.
	 * \param signature the signature example "48 ? ? ?"
	 * \param range_start Region to start scan from 
	 * \param range_end Region up to where it should scan
	 * \param PID (OPTIONAL) where to read to?
	 * \return address of signature
	 */
	uint64_t FindSignature(const char* signature, uint64_t range_start, uint64_t range_end, int PID = 0);

	/**
	 * \brief Writes memory to the process 
	 * \param address The address to write to
	 * \param buffer The buffer to write
	 * \param size The size of the buffer
	 * \return 
	 */
	bool Write(uintptr_t address, void* buffer, size_t size) const;
	bool Write(uintptr_t address, void* buffer, size_t size, int pid) const;

	/**
	 * \brief Writes memory to the process using a template
	 * \param address to write to
	 * \param value the value you'll write to the address
	 */
	template <typename T>
	void Write(void* address, T value)
	{
		Write(address, &value, sizeof(T));
	}

	template <typename T>
	void Write(uintptr_t address, T value)
	{
		Write(address, &value, sizeof(T));
	}

	/**
	* brief Reads memory from the process
	* @param address The address to read from
	* @param buffer The buffer to read to
	* @param size The size of the buffer
	* @return true if successful, false if not.
	*/
	bool Read(uintptr_t address, void* buffer, size_t size) const;
	bool Read(uintptr_t address, void* buffer, size_t size, int pid) const;

	/**
	* brief Reads memory from the process using a template
	* @param address The address to read from
	* @return the value read from the process
	*/
	template <typename T>
	T Read(void* address)
	{
		T buffer { };
		memset(&buffer, 0, sizeof(T));
		Read(reinterpret_cast<uint64_t>(address), reinterpret_cast<void*>(&buffer), sizeof(T));

		return buffer;
	}

	template <typename T>
	T Read(uint64_t address)
	{
		return Read<T>(reinterpret_cast<void*>(address));
	}

	/**
	* brief Reads memory from the process using a template and pid
	* @param address The address to read from
	* @param pid The process id of the process
	* @return the value read from the process
	*/
	template <typename T>
	T Read(void* address, int pid)
	{
		T buffer { };
		memset(&buffer, 0, sizeof(T));
		Read(reinterpret_cast<uint64_t>(address), reinterpret_cast<void*>(&buffer), sizeof(T), pid);

		return buffer;
	}

	template <typename T>
	T Read(uint64_t address, int pid)
	{
		return Read<T>(reinterpret_cast<void*>(address), pid);
	}

	/**
	* brief Reads a chain of offsets from the address
	* @param address The address to read from
	* @param a vector of offset values to read through
	* @return the value read from the chain
	*/
	uint64_t ReadChain(uint64_t base, const std::vector<uint64_t>& offsets)
	{
		uint64_t result = Read<uint64_t>(base + offsets.at(0));
		for (size_t i = 1; i < offsets.size(); i++) result = Read<uint64_t>(result + offsets.at(i));
		return result;
	}

	template <typename T>
	T ReadChain(uint64_t base, const std::vector<uint64_t>& offsets)
	{
		T result = Read<T>(base + offsets.at(0));
		for (size_t i = 1; i < offsets.size(); i++) result = Read<T>(result + offsets.at(i));
		return result;
	}

	/**
	 * \brief Create a scatter handle, this is used for scatter read/write requests
	 * \return Scatter handle
	 */
	VMMDLL_SCATTER_HANDLE CreateScatterHandle() const;
	VMMDLL_SCATTER_HANDLE CreateScatterHandle(int pid) const;

	/**
	 * \brief Closes the scatter handle
	 * \param handle
	 */
	void CloseScatterHandle(VMMDLL_SCATTER_HANDLE handle);

	/**
	 * \brief Adds a scatter read/write request to the handle
	 * \param handle the handle
	 * \param address the address to read/write to 
	 * \param buffer the buffer to read/write to
	 * \param size the size of buffer
	 */
	void AddScatterReadRequest(VMMDLL_SCATTER_HANDLE handle, uint64_t address, void* buffer, size_t size);

	template <typename T>
	void AddScatterReadRequest(VMMDLL_SCATTER_HANDLE handle, uint64_t address, T* buffer)
	{
		AddScatterReadRequest(handle, address, reinterpret_cast<void*>(buffer), sizeof(T));
	}
		
	void AddScatterWriteRequest(VMMDLL_SCATTER_HANDLE handle, uint64_t address, void* buffer, size_t size);
		

	/**
	 * \brief Executes all prepared scatter requests, note if you created a scatter handle with a pid
	 * you'll need to specify the pid in the execute function. so we can clear the scatters from the handle.
	 * \param handle 
	 * \param pid 
	 */
	bool ExecuteReadScatter(VMMDLL_SCATTER_HANDLE handle, int pid = 0);
	void ExecuteWriteScatter(VMMDLL_SCATTER_HANDLE handle, int pid = 0);
	void SetScatterReadWarningSuppressed(bool suppressed);

	/*the FPGA handle*/
	VMM_HANDLE vHandle;
};

inline Memory mem;
