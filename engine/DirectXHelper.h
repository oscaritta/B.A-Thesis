#pragma once

namespace DXHELP
{
	// Driver types supported
	D3D_DRIVER_TYPE gDriverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE
	};
	UINT gNumDriverTypes = ARRAYSIZE(gDriverTypes);

	// Feature levels supported
	D3D_FEATURE_LEVEL gFeatureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_1
	};
	UINT gNumFeatureLevels = ARRAYSIZE(gFeatureLevels);

	class DirectXHelper
	{
	public:
		CComPtrCustom<ID3D11Device> lDevice = nullptr;
		CComPtrCustom<ID3D11DeviceContext> lImmediateContext = nullptr;
		CComPtrCustom<IDXGIOutputDuplication> lDeskDupl = nullptr;
		CComPtrCustom<ID3D11Texture2D> lAcquiredDesktopImage = nullptr;
		CComPtrCustom<ID3D11Texture2D> lGDIImage = nullptr;
		CComPtrCustom<ID3D11Texture2D> lDestImage = nullptr;
		DXGI_OUTPUT_DESC lOutputDesc;
		DXGI_OUTDUPL_DESC lOutputDuplDesc; /*
				The DXGI_OUTDUPL_DESC structure describes the dimension of the output and the surface that contains the desktop image. 
				Width,Height,RefreshRate,Format(always DXGI_FORMAT_B8G8R8A8_UNORM.),ScanlineOrdering,Scaling
										   */
		CComPtrCustom<IDXGIDevice> lDxgiDevice;
		CComPtrCustom<IDXGIAdapter> lDxgiAdapter;

		// Get Direct3D Device and Direct3D Immediate Context
		// Immediate Context is used to copy a ID3D11Texture2D onto another ID3D11Texture2D
		// Immediate Context is also used to copy a ID3D11Texture2D onto a resource(RAM memory/buffer)
		// Immediate Context calls : CopyResource(lGDIImage, lAcquiredDesktopImage),
		//							CopyResource(lDestImage, lGDIImage);
		//							Map(lDestImage, subresource, D3D11_MAP_READ_WRITE, 0, &resource);
		HRESULT initD3D()
		{
			D3D_FEATURE_LEVEL lFeatureLevel;
			HRESULT hr(E_FAIL);
			// Create device
			for (UINT DriverTypeIndex = 0; DriverTypeIndex < gNumDriverTypes; ++DriverTypeIndex)
			{
				hr = D3D11CreateDevice(
					nullptr,
					gDriverTypes[DriverTypeIndex],
					nullptr,
					0,
					gFeatureLevels,
					gNumFeatureLevels,
					D3D11_SDK_VERSION,
					&lDevice,
					&lFeatureLevel,
					&lImmediateContext);

				if (SUCCEEDED(hr)) return hr;

				lDevice.Release();
				lImmediateContext.Release();
			}
			return hr;
		}
		
		// 1. Query Direct3D Device to get DXGI Device
		// 2. Get DXGI Adapter by Calling GetParent from DXGI Device
		// 3. Release DXGI Device
		// "The IDXGIAdapter interface represents a display subsystem (including one or more GPUs, DACs and video memory)."
		HRESULT getDXGIAdapter()
		{
			// Get DXGI device
			HRESULT hr(E_FAIL);
			hr = lDevice->QueryInterface(IID_PPV_ARGS(&lDxgiDevice));

			if (FAILED(hr)) return hr;

			// Get DXGI adapter
			hr = lDxgiDevice->GetParent(
				__uuidof(IDXGIAdapter),
				reinterpret_cast<void**>(&lDxgiAdapter));

			lDxgiDevice.Release();
			return hr;
		}

		// "An IDXGIOutput interface represents an adapter output (such as a monitor)."
		// The Adapter(Video Card) can give us the Output(Monitor)
		// The difference between IDXGIOutput and IDXGIOutput1 is that
		// IDXGIOutput1 has an additional method, which is ---DuplicateOutput----
		HRESULT getDXGIOutput()
		{
			// Get output
			HRESULT hr(E_FAIL);
			CComPtrCustom<IDXGIOutput> lDxgiOutput; 
			UINT Output = 0;
			hr = lDxgiAdapter->EnumOutputs(Output,
				&lDxgiOutput);
			
			if (FAILED(hr)) return hr;

			lDxgiAdapter.Release();
			hr = lDxgiOutput->GetDesc(
				&lOutputDesc); // OutputDesc -> info such as Device Name, Monitor, Rotation
			
			if (FAILED(hr)) return hr;

			// QI for Output 1
			CComPtrCustom<IDXGIOutput1> lDxgiOutput1;
			hr = lDxgiOutput->QueryInterface(IID_PPV_ARGS(&lDxgiOutput1));
			
			if (FAILED(hr)) return hr;
			// Create desktop duplication
			hr = lDxgiOutput1->DuplicateOutput(lDevice, &lDeskDupl);
			
			if (FAILED(hr)) return hr;

			lDxgiOutput.Release();
			lDxgiOutput1.Release();
			return hr;
		}

		HRESULT createTextures()
		{
			HRESULT hr(E_FAIL);
			// Create GUI drawing texture
			lDeskDupl->GetDesc(&lOutputDuplDesc); // Gives info like Width,Height,Format of the Desktop Image
			
			D3D11_TEXTURE2D_DESC desc;
			desc.Width = lOutputDuplDesc.ModeDesc.Width;
			desc.Height = lOutputDuplDesc.ModeDesc.Height;
			desc.Format = lOutputDuplDesc.ModeDesc.Format;
			desc.ArraySize = 1;
			desc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_RENDER_TARGET;
			desc.MiscFlags = D3D11_RESOURCE_MISC_GDI_COMPATIBLE; // GDI-compatible, we need to draw on this texture the cursor and other debug lines
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.MipLevels = 1;
			desc.CPUAccessFlags = 0; // no need for cpu access for a texture from the gpu
			desc.Usage = D3D11_USAGE_DEFAULT; // put it in the VRAM, the CPU can't access it. 
			
			hr = lDevice->CreateTexture2D(&desc, NULL, &lGDIImage);

			if (FAILED(hr)) return hr;
			if (lGDIImage == nullptr) return E_FAIL;

			// Create CPU access texture
			desc.Width = lOutputDuplDesc.ModeDesc.Width;
			desc.Height = lOutputDuplDesc.ModeDesc.Height;
			desc.Format = lOutputDuplDesc.ModeDesc.Format;
			desc.ArraySize = 1;
			desc.BindFlags = 0;
			desc.MiscFlags = 0;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.MipLevels = 1;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE; // Enable CPU I/O
			desc.Usage = D3D11_USAGE_STAGING; //  means put it in system memory, and the GPU can't access it.

			hr = lDevice->CreateTexture2D(&desc, NULL, &lDestImage); // The final texture, from which we will get the bitmap

			if (FAILED(hr)) return hr;
			if (lDestImage == nullptr) return E_FAIL;

			return hr;
		}


	};
}