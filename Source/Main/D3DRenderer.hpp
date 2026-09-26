#pragma once
#include "../Common.hpp"

namespace ER
{
    struct TextureFileData
    {
        unsigned char* data = nullptr;
        int width = 0;
        int height = 0;

        static inline std::string bossBarFile = "PostureBarResources\\BossBar.png";
        static inline std::string bossBorderFile = "PostureBarResources\\BossBarBorder.png";
        static inline std::string entityBarFile = "PostureBarResources\\EntityBar.png";
        static inline std::string entityBorderFile = "PostureBarResources\\EntityBarBorder.png";
        static inline std::string circleBorderFile = "PostureBarResources\\CircleBorder.png";

        static inline std::string poisonFile = "PostureBarResources\\StatusIcons\\poison.png";
        static inline std::string rotFile = "PostureBarResources\\StatusIcons\\rot.png";
        static inline std::string bleedFile = "PostureBarResources\\StatusIcons\\bleed.png";
        static inline std::string blightFile = "PostureBarResources\\StatusIcons\\blight.png";
        static inline std::string frostFile = "PostureBarResources\\StatusIcons\\frost.png";
        static inline std::string sleepFile = "PostureBarResources\\StatusIcons\\sleep.png";
        static inline std::string madnessFile = "PostureBarResources\\StatusIcons\\madness.png";
    };

    struct D3D12TextureData
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
        ID3D12Resource* dx12Resource = nullptr;
        int width = 0;
        int height = 0;
    };

    class D3DRenderer
    {
        class ProgramData
        {
        public:
            explicit ProgramData();
            ~ProgramData() noexcept = default;
            ProgramData(ProgramData const&) = delete;
            ProgramData(ProgramData&&) = delete;
            ProgramData& operator=(ProgramData const&) = delete;
            ProgramData& operator=(ProgramData&&) = delete;

            //    Dx & ImGui
            int m_GamePid{};
            HANDLE m_GameHandle{};
            HWND m_GameWindow{};
            int m_GameWidth{ 0 };
            int m_GameHeight{ 0 };
            std::string m_GameTitle{};
            std::string m_ClassName{};
            std::string m_GamePath{};
            uintptr_t m_ModuleBase{};
        };

        typedef HRESULT(APIENTRY* Present12) (IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
        Present12 oPresent = NULL;

        typedef void(APIENTRY* ExecuteCommandLists)(ID3D12CommandQueue* queue, UINT NumCommandLists, ID3D12CommandList* ppCommandLists);
        ExecuteCommandLists oExecuteCommandLists = NULL;

        typedef HRESULT(APIENTRY* ResizeTarget)(IDXGISwapChain* _this, const DXGI_MODE_DESC* pNewTargetParameters);
        ResizeTarget oResizeTarget = NULL;

    public:
        explicit D3DRenderer()
            : programData(std::make_unique<ProgramData>()) {};
        ~D3DRenderer() noexcept;
        D3DRenderer(D3DRenderer const&) = delete;
        D3DRenderer(D3DRenderer&&) = delete;
        D3DRenderer& operator=(D3DRenderer const&) = delete;
        D3DRenderer& operator=(D3DRenderer&&) = delete;

        bool m_Init = false;
        bool m_InInternalTextureUpload = false;


        void Overlay(IDXGISwapChain* pSwapChain);

        static HRESULT APIENTRY HookPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
        static void HookExecuteCommandLists(ID3D12CommandQueue* queue, UINT NumCommandLists, ID3D12CommandList* ppCommandLists);
        static HRESULT APIENTRY HookResizeTarget(IDXGISwapChain* _this, const DXGI_MODE_DESC* pNewTargetParameters);
        void ResetRenderState(IDXGISwapChain3* swapChain = nullptr);

        bool loadTextureFileData(const std::string& filename, TextureFileData* textureFileData);
        void loadBarTextures();

        void EnableDebugLayer();
        bool InitHook();
        bool Hook();
        void Unhook();

        bool InitWindow();
        bool DeleteWindow();

        bool CreateHook(uint16_t Index, void** Original, void* Function);
        void DisableHook(uint16_t Index);
        void DisableAll();

        static LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

        uint64_t m_OldWndProc{};

    private:

        std::unique_ptr<ProgramData> programData;

        WNDCLASSEX WindowClass{};
        HWND WindowHwnd{};

        IDXGISwapChain3* m_Swapchain{};
        ID3D12Device* m_Device{};

        TextureFileData entityBarFileData;
        TextureFileData entityBarBorderFileData;
        TextureFileData bossBarFileData;
        TextureFileData bossBarBorderFileData;
        TextureFileData circleBorderFileData;
        TextureFileData statusIconFileData[7];
        bool statusIconsLoaded[7] = { false };
        bool textureFileDataLoaded = false;

        ID3D12DescriptorHeap* m_srvDescriptorHeap{};
        ID3D12DescriptorHeap* m_rtvDescriptorHeap{};
        ID3D12CommandAllocator** m_CommandAllocators;
        ID3D12GraphicsCommandList* m_CommandList{};
        ID3D12CommandQueue* m_CommandQueue{};
        ID3D12Resource** m_BackBuffers;
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_RenderTargets;

        uint64_t m_BuffersCounts = -1;
        // 1 + number_of_textures * 2(cpu + gpu handles)
        static inline const int srvDescriptorsNumWithTextures = 25;

        bool IsInWorld();

        IDXGISwapChain* m_CurrentSwapChain = nullptr;
        ID3D12Fence* m_Fence = nullptr;
        UINT64 m_FenceValue = 0;
        HANDLE m_FenceEvent = nullptr;
        UINT64* m_BufferFenceValues = nullptr;
    };

    HWND FindERWindow();
    inline std::unique_ptr<D3DRenderer> g_D3DRenderer;
}