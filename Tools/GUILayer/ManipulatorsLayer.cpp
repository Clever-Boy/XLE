// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ManipulatorsLayer.h"

#include "LevelEditorScene.h"
#include "ExportedNativeTypes.h"
#include "MarshalString.h"

#include "../ToolsRig/TerrainManipulators.h"
#include "../ToolsRig/PlacementsManipulators.h"
#include "../ToolsRig/IManipulator.h"

namespace GUILayer
{
    TerrainManipulatorsPimpl::RegisteredManipulator::~RegisteredManipulator() {}

	clix::shared_ptr<ToolsRig::IManipulator> TerrainManipulators::GetManipulator(System::String^ name)
	{
		auto nativeName = clix::marshalString<clix::E_UTF8>(name);
		for (const auto& i : _pimpl->_terrainManipulators)
			if (i._name == nativeName) return clix::shared_ptr<ToolsRig::IManipulator>(i._manipulator);
		return clix::shared_ptr<ToolsRig::IManipulator>();
	}

	System::Collections::Generic::IEnumerable<System::String^>^ TerrainManipulators::GetManipulatorNames()
	{
		auto result = gcnew System::Collections::Generic::List<System::String^>();
		for (const auto& i : _pimpl->_terrainManipulators)
			result->Add(clix::marshalString<clix::E_UTF8>(i._name));
		return result;
	}

    TerrainManipulators::TerrainManipulators(std::shared_ptr<SceneEngine::TerrainManager> terrain)
    {
        _pimpl.reset(new TerrainManipulatorsPimpl);

        auto manip = ToolsRig::CreateTerrainManipulators(terrain);
        for (auto& t : manip) {
            _pimpl->_terrainManipulators.push_back(
                TerrainManipulatorsPimpl::RegisteredManipulator(t->GetName(), std::move(t)));
        }
    }

    TerrainManipulators::~TerrainManipulators() 
    {
        _pimpl.reset();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    PlacementManipulatorsPimpl::RegisteredManipulator::~RegisteredManipulator() {}

	clix::shared_ptr<ToolsRig::IManipulator> PlacementManipulators::GetManipulator(System::String^ name)
	{
		auto nativeName = clix::marshalString<clix::E_UTF8>(name);
		for (const auto& i : _pimpl->_manipulators)
			if (i._name == nativeName) return clix::shared_ptr<ToolsRig::IManipulator>(i._manipulator);
		return clix::shared_ptr<ToolsRig::IManipulator>();
	}

	System::Collections::Generic::IEnumerable<System::String^>^ PlacementManipulators::GetManipulatorNames()
	{
		auto result = gcnew System::Collections::Generic::List<System::String^>();
		for (const auto& i : _pimpl->_manipulators)
			result->Add(clix::marshalString<clix::E_UTF8>(i._name));
		return result;
	}

    PlacementManipulators::PlacementManipulators(
        ToolsRig::IPlacementManipulatorSettings* context,
        std::shared_ptr<SceneEngine::PlacementsEditor> editor)
    {
        _pimpl.reset(new PlacementManipulatorsPimpl);

        auto manip = ToolsRig::CreatePlacementManipulators(context, editor);
        for (auto& t : manip) {
            _pimpl->_manipulators.push_back(
                PlacementManipulatorsPimpl::RegisteredManipulator(t->GetName(), std::move(t)));
        }
    }

    PlacementManipulators::~PlacementManipulators() 
    {
        _pimpl.reset();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    namespace Internal
    {
        class PlacementManipulatorSettingsAdapter : public ToolsRig::IPlacementManipulatorSettings
        {
        public:
            virtual std::string GetSelectedModel() const;
            virtual void EnableSelectedModelDisplay(bool newState);
            virtual void SelectModel(const char newModelName[]);
            virtual void SwitchToMode(Mode::Enum newMode);

            PlacementManipulatorSettingsAdapter(IPlacementManipulatorSettingsLayer^ managed);
            virtual ~PlacementManipulatorSettingsAdapter();
        private:
            gcroot<IPlacementManipulatorSettingsLayer^> _managed;
        };

        std::string PlacementManipulatorSettingsAdapter::GetSelectedModel() const
        {
            return clix::marshalString<clix::E_UTF8>(_managed->GetSelectedModel());
        }
        void PlacementManipulatorSettingsAdapter::EnableSelectedModelDisplay(bool newState)
        {
            return _managed->EnableSelectedModelDisplay(newState);
        }
        void PlacementManipulatorSettingsAdapter::SelectModel(const char newModelName[])
        {
            _managed->SelectModel(
                clix::marshalString<clix::E_UTF8>(newModelName));
        }
        void PlacementManipulatorSettingsAdapter::SwitchToMode(Mode::Enum newMode)
        {
            _managed->SwitchToMode((unsigned)newMode);
        }

        PlacementManipulatorSettingsAdapter::PlacementManipulatorSettingsAdapter(
            IPlacementManipulatorSettingsLayer^ managed)
        {
            _managed = managed;
        }

        PlacementManipulatorSettingsAdapter::~PlacementManipulatorSettingsAdapter()
        {}
    }

    ToolsRig::IPlacementManipulatorSettings* IPlacementManipulatorSettingsLayer::GetNative()
    {
        return _native.get();
    }
        
    IPlacementManipulatorSettingsLayer::IPlacementManipulatorSettingsLayer()
    {
        _native.reset(new Internal::PlacementManipulatorSettingsAdapter(this));
    }

    IPlacementManipulatorSettingsLayer::~IPlacementManipulatorSettingsLayer()
    {
        _native.get();
    }
}
