﻿// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Xml;
using System.Xml.Schema;

using Sce.Atf;
using Sce.Atf.Dom;
using Sce.Atf.Adaptation;
using Sce.Atf.Controls.PropertyEditing;

namespace LevelEditorXLE
{
    // Theses are functions that are difficult to fit within the 
    // architecture in a better way. These functions are explicitly
    // called from the "LevelEditor" project. But ideally we'd have
    // some better way to attach a library like this, without
    // having to change the LevelEditor project so much.
    public static class Patches
    {
        public static DomNode GetReferenceTarget(DomNode node)
        {
            var placementsCellRef = node.As<Placements.PlacementsCellRef>();
            if (placementsCellRef != null && placementsCellRef.Target != null)
            {
                node = placementsCellRef.Target.As<DomNode>();
            }
            return node;
        }

        public static bool IsReferenceType(DomNodeType nodeType)
        {
            return Schema.placementsCellReferenceType.Type.IsAssignableFrom(nodeType);
        }

        public static void ResolveOnLoad(IAdaptable gameNode)
        {
            var game = gameNode.As<Game.GameExtensions>();
            if (game == null) return;

            var placementsFolder = game.PlacementsFolder;
            if (placementsFolder != null)
            {
                foreach (var cell in placementsFolder.Cells)
                {
                    cell.Resolve();
                }
            }
        }

        public static void SaveReferencedDocuments(IAdaptable gameNode)
        {
            var game = gameNode.As<Game.GameExtensions>();
            if (game == null) return;

            var placementsFolder = game.PlacementsFolder;
            if (placementsFolder != null)
            {
                foreach (var cellRef in placementsFolder.Cells)
                {
                    var doc = cellRef.Target;
                    if (doc == null) continue;
                    System.Diagnostics.Debug.Assert(false, "Incomplete implementation");
                    // SaveDomDocument(doc.DomNode, cellRef.Uri, schemaLoader);
                }
            }
        }

        public static Tuple<string, string> GetSchemaResourceName()
        {
            return new Tuple<string, string>("LevelEditorXLE.Schema", "xleroot.xsd");
        }

        public static void OnSchemaSetLoaded(
            XmlSchemaSet schemaSet, 
            IEnumerable<XmlSchemaTypeCollection> typeCollections)
        {
            foreach (XmlSchemaTypeCollection typeCollection in typeCollections)
            {
                Schema.Initialize(typeCollection);
                Startup.InitializeAdapters();
                break;
            }

            var strataCollectionEditor = new EmbeddedCollectionEditor();

            strataCollectionEditor.GetItemInsertersFunc = (context) =>
            {
                var list = context.GetValue() as IList<DomNode>;
                if (list == null) return EmptyArray<EmbeddedCollectionEditor.ItemInserter>.Instance;

                // create ItemInserter for each component type.
                var insertors = new EmbeddedCollectionEditor.ItemInserter[1]
                    {
                        new EmbeddedCollectionEditor.ItemInserter("Strata",
                        delegate
                        {
                            DomNode node = new DomNode(Schema.terrainBaseTextureStrataType.Type);
                            list.Add(node);
                            return node;
                        })
                    };
                return insertors;
            };

            strataCollectionEditor.RemoveItemFunc = (context, item) =>
            {
                var list = context.GetValue() as IList<DomNode>;
                if (list != null)
                    list.Remove(item.Cast<DomNode>());
            };

            strataCollectionEditor.MoveItemFunc = (context, item, delta) =>
            {
                var list = context.GetValue() as IList<DomNode>;
                if (list != null)
                {
                    DomNode node = item.Cast<DomNode>();
                    int index = list.IndexOf(node);
                    int insertIndex = index + delta;
                    if (insertIndex < 0 || insertIndex >= list.Count)
                        return;
                    list.RemoveAt(index);
                    list.Insert(insertIndex, node);
                }
            };

            Schema.terrainBaseTextureType.Type.SetTag(
                   new PropertyDescriptorCollection(
                       new System.ComponentModel.PropertyDescriptor[] {
                            new ChildPropertyDescriptor(
                                "Strata".Localize(),
                                Schema.terrainBaseTextureType.strataChild,
                                null,
                                "List of texturing stratas".Localize(),
                                false,
                                strataCollectionEditor)
                                }));
        }

        public static DomNodeType GetGameType() { return Schema.xleGameType.Type; }
    }
}