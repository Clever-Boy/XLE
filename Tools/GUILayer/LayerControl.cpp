// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4793) //  : function compiled as native :

#include "LayerControl.h"
#include "IWindowRig.h"
#include "IOverlaySystem.h"
#include "UITypesBinding.h"
#include "EngineDevice.h"
#include "NativeEngineDevice.h"
#include "GUILayerUtil.h"
#include "ExportedNativeTypes.h"
#include "../ToolsRig/ModelVisualisation.h"
#include "../ToolsRig/IManipulator.h"
#include "../ToolsRig/BasicManipulators.h"
#include "../ToolsRig/VisualisationUtils.h"
#include "../../PlatformRig/InputTranslator.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../PlatformRig/OverlaySystem.h"

#include "../../RenderOverlays/DebuggingDisplay.h"
#include "../../RenderOverlays/Font.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../SceneEngine/LightingParserStandardPlugin.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../SceneEngine/VegetationSpawn.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Assets/Material.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/StringFormat.h"
#include <stack>
#include <iomanip>

unsigned FrameRenderCount = 0;

namespace GUILayer 
{
    
    static PlatformRig::FrameRig::RenderResult RenderFrame(
        RenderCore::IThreadContext& context,
        LayerControlPimpl& pimpl,
        PlatformRig::IOverlaySystem* overlaySys)
    {
        using namespace SceneEngine;

        context.ClearAllBoundTargets();

        LightingParserContext lightingParserContext(*pimpl._globalTechniqueContext);
        lightingParserContext._plugins.push_back(pimpl._stdPlugin);

        if (overlaySys) {
            overlaySys->RenderToScene(&context, lightingParserContext);
        }

        ///////////////////////////////////////////////////////////////////////
        bool hasPendingMessage = lightingParserContext.HasPendingAssets() || lightingParserContext.HasInvalidAssets() || lightingParserContext.HasErrorString();
        if (hasPendingMessage) {
            auto metalContext = RenderCore::Metal::DeviceContext::Get(context);
            auto defaultFont0 = RenderOverlays::GetX2Font("Raleway", 16);
            DrawPendingResources(metalContext.get(), lightingParserContext, defaultFont0.get());
        }
        ///////////////////////////////////////////////////////////////////////

        if (overlaySys) {
            overlaySys->RenderWidgets(&context, lightingParserContext.GetProjectionDesc());
        }

        return PlatformRig::FrameRig::RenderResult(lightingParserContext.HasPendingAssets());
    }

    bool LayerControl::Render(RenderCore::IThreadContext& threadContext, IWindowRig& windowRig)
    {
            // Rare cases can recursively start rendering
            // (for example, if we attempt to call a Windows GUI function while in the middle of
            // rendering)
            // Re-entering rendering recursively can cause some bad problems, however
            //  -- so we need to prevent it.
        if (_pimpl->_activePaint)
            return false;

            // Check for cases where a paint operation can be begun on one window
            // while another window is in the middle of rendering.
        static bool activePaintCheck2 = false;
        if (activePaintCheck2) return false;

        _pimpl->_activePaint = true;
        activePaintCheck2 = true;
        
        bool result = true;
        TRY
        {
            auto& frameRig = windowRig.GetFrameRig();
            auto frResult = frameRig.ExecuteFrame(
                threadContext, windowRig.GetPresentationChain().get(), 
                nullptr, nullptr,
                std::bind(
                    RenderFrame, std::placeholders::_1,
                    std::ref(*_pimpl), frameRig.GetMainOverlaySystem().get()));

            // return false if when we have pending resources (encourage another redraw)
            result =  !frResult._renderResult._hasPendingResources;
        } CATCH (...) {
        } CATCH_END
        activePaintCheck2 = false;
        _pimpl->_activePaint = false;

        return result;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class InputLayer : public PlatformRig::IOverlaySystem
    {
    public:
        std::shared_ptr<IInputListener> GetInputListener();

        void RenderToScene(
            RenderCore::IThreadContext* context, 
            SceneEngine::LightingParserContext& parserContext); 
        void RenderWidgets(
            RenderCore::IThreadContext* context, 
            const RenderCore::Techniques::ProjectionDesc& projectionDesc);
        void SetActivationState(bool newState);

        InputLayer(std::shared_ptr<IInputListener> listener);
        ~InputLayer();
    protected:
        std::shared_ptr<IInputListener> _listener;
    };

    auto InputLayer::GetInputListener() -> std::shared_ptr<IInputListener>
    {
        return _listener;
    }

    void InputLayer::RenderToScene(
        RenderCore::IThreadContext*, 
        SceneEngine::LightingParserContext&) {}
    void InputLayer::RenderWidgets(
        RenderCore::IThreadContext*, 
        const RenderCore::Techniques::ProjectionDesc&) {}
    void InputLayer::SetActivationState(bool) {}

    InputLayer::InputLayer(std::shared_ptr<IInputListener> listener) : _listener(listener) {}
    InputLayer::~InputLayer() {}

///////////////////////////////////////////////////////////////////////////////////////////////////
    
    public ref class VisResources
    {
    public:
        clix::shared_ptr<RenderCore::Assets::ModelCache> _visCache;

        VisResources();
        ~VisResources();
        !VisResources();
    };

    VisResources::VisResources()
    {
        _visCache = std::make_shared<RenderCore::Assets::ModelCache>();
    }

    VisResources::~VisResources() {}
    VisResources::!VisResources() { /* System::Diagnostics::Debug::Assert(false, "Non deterministic delete of VisResources"); */ }

    static void RenderTrackingOverlay(
        RenderOverlays::IOverlayContext& context,
        const ToolsRig::VisMouseOver& mouseOver,
        std::shared_ptr<ToolsRig::ModelVisSettings> modelSettings,
        std::shared_ptr<RenderCore::Assets::ModelCache> modelCache)
    {
        using namespace RenderOverlays::DebuggingDisplay;
        auto viewportDims = context.GetDeviceContext()->GetStateDesc()._viewportDimensions;

        auto textHeight = (int)context.TextHeight(nullptr);
        String^ matName = VisMouseOver::DescriptiveMaterialName(
            VisMouseOver::BuildFullMaterialName(*modelSettings, *modelCache, mouseOver._materialGuid));
        DrawText(
            &context,
            Rect(Coord2(3, viewportDims[1]-textHeight-3), Coord2(viewportDims[0]-3, viewportDims[1]-3)),
            nullptr, RenderOverlays::ColorB(0xffafafaf),
            StringMeld<512>() 
                << "Material: {Color:7f3faf}" << clix::marshalString<clix::E_UTF8>(matName)
                << "{Color:afafaf}, Draw call: " << mouseOver._drawCallIndex
                << std::setprecision(4)
                << ", (" << mouseOver._intersectionPt[0]
                << ", "  << mouseOver._intersectionPt[1]
                << ", "  << mouseOver._intersectionPt[2]
                << ")");
    }

    void LayerControl::SetupDefaultVis(ModelVisSettings^ settings, VisMouseOver^ mouseOver, VisResources^ resources)
    {
        auto visLayer = std::make_unique<ToolsRig::ModelVisLayer>(
            settings->GetUnderlying(), resources->_visCache.GetNativePtr());
        auto& overlaySet = *GetWindowRig().GetFrameRig().GetMainOverlaySystem();
        overlaySet.AddSystem(std::move(visLayer));
        overlaySet.AddSystem(
            std::make_shared<ToolsRig::VisualisationOverlay>(
                settings->GetUnderlying(), resources->_visCache.GetNativePtr(), 
                mouseOver ? mouseOver->GetUnderlying() : nullptr));

        auto immContext = EngineDevice::GetInstance()->GetNative().GetRenderDevice()->GetImmediateContext();

        auto intersectionScene = CreateModelIntersectionScene(
            settings->GetUnderlying(), resources->_visCache.GetNativePtr());
        auto intersectionContext = std::make_shared<SceneEngine::IntersectionTestContext>(
            immContext,
            RenderCore::Techniques::CameraDesc(), 
            GetWindowRig().GetPresentationChain()->GetViewportContext(),
            _pimpl->_globalTechniqueContext);

        // AddDefaultCameraHandler(settings->Camera);
        {
            auto manipulators = std::make_unique<ToolsRig::ManipulatorStack>(intersectionContext, intersectionScene);
            manipulators->Register(
                ToolsRig::ManipulatorStack::CameraManipulator,
                ToolsRig::CreateCameraManipulator(settings->Camera->GetUnderlying()));
            overlaySet.AddSystem(std::make_shared<InputLayer>(std::move(manipulators)));
        }

        using namespace std::placeholders;
        overlaySet.AddSystem(
            std::make_shared<ToolsRig::MouseOverTrackingOverlay>(
                mouseOver->GetUnderlying(),
                immContext,
                _pimpl->_globalTechniqueContext,
                settings->Camera->GetUnderlying(), intersectionScene,
                std::bind(&RenderTrackingOverlay, _1, _2, settings->GetUnderlying(), resources->_visCache.GetNativePtr())));
    }

    VisMouseOver^ LayerControl::CreateVisMouseOver(ModelVisSettings^ settings, VisResources^ resources)
    {
        return gcnew VisMouseOver(
            std::make_shared<ToolsRig::VisMouseOver>(), settings->GetUnderlying(), resources->_visCache.GetNativePtr());
    }

    VisResources^ LayerControl::CreateVisResources()
    {
        return gcnew VisResources();
    }

    void LayerControl::SetUpdateAsyncMan(bool updateAsyncMan)
    {
        GetWindowRig().GetFrameRig().SetUpdateAsyncMan(updateAsyncMan);
    }

    namespace Internal
    {
        class OverlaySystemAdapter : public PlatformRig::IOverlaySystem
        {
        public:
            typedef RenderOverlays::DebuggingDisplay::IInputListener IInputListener;

            std::shared_ptr<IInputListener> GetInputListener()
            {
                // return _managedOverlay->GetInputListener();
                return nullptr;
            }

            void RenderToScene(
                RenderCore::IThreadContext* device, 
                SceneEngine::LightingParserContext& parserContext)
            {
                _managedOverlay->RenderToScene(device, parserContext);
            }
            
            void RenderWidgets(
                RenderCore::IThreadContext* device, 
                const RenderCore::Techniques::ProjectionDesc& projectionDesc)
            {
                _managedOverlay->RenderWidgets(device, projectionDesc);
            }

            void SetActivationState(bool newState)
            {
                _managedOverlay->SetActivationState(newState);
            }

            OverlaySystemAdapter(::GUILayer::IOverlaySystem^ managedOverlay) : _managedOverlay(managedOverlay) {}
            ~OverlaySystemAdapter() {}
        protected:
            gcroot<::GUILayer::IOverlaySystem^> _managedOverlay;
        };
    }

    void LayerControl::AddSystem(IOverlaySystem^ overlay)
    {
        auto& overlaySet = *GetWindowRig().GetFrameRig().GetMainOverlaySystem();
        overlaySet.AddSystem(std::shared_ptr<Internal::OverlaySystemAdapter>(
            new Internal::OverlaySystemAdapter(overlay)));
    }

    void LayerControl::AddDefaultCameraHandler(VisCameraSettings^ settings)
    {
            // create an input listener that feeds into a stack of manipulators
        auto manipulators = std::make_unique<ToolsRig::ManipulatorStack>();
        manipulators->Register(
            ToolsRig::ManipulatorStack::CameraManipulator,
            ToolsRig::CreateCameraManipulator(settings->GetUnderlying()));

        auto& overlaySet = *GetWindowRig().GetFrameRig().GetMainOverlaySystem();
        overlaySet.AddSystem(std::make_shared<InputLayer>(std::move(manipulators)));
    }

    TechniqueContextWrapper^ LayerControl::GetTechniqueContext()
    {
        return _techContextWrapper;
    }

    LayerControl::LayerControl(Control^ control)
        : EngineControl(control)
    {
        _pimpl.reset(new LayerControlPimpl());
        _pimpl->_stdPlugin = std::make_shared<SceneEngine::LightingParserStandardPlugin>();
        _pimpl->_globalTechniqueContext = std::make_shared<RenderCore::Techniques::TechniqueContext>();
        _techContextWrapper = gcnew TechniqueContextWrapper(_pimpl->_globalTechniqueContext);
        _pimpl->_activePaint = false;
    }

    LayerControl::~LayerControl() 
    {
        delete _techContextWrapper;
    }

    LayerControl::!LayerControl()
    {
        // System::Diagnostics::Debug::Assert(false, "Non deterministic delete of LayerControl");
    }
}

