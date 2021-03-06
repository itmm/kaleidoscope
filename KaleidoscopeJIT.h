#pragma once

#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/RTDyldMemoryManager.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Mangler.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include <string>
#include <vector>

namespace llvm { namespace orc {

	class KaleidoscopeJIT {
		public:
			using ObjLayerT = LegacyRTDyldObjectLinkingLayer;
			using CompileLayerT = LegacyIRCompileLayer<ObjLayerT, SimpleCompiler>;

			KaleidoscopeJIT() :
				Resolver(createLegacyLookupResolver(
					ES,
					[this](const llvm::StringRef Name) {
						return findMangledSymbol((std::string) Name);
					},
					[](Error Err) {
						cantFail(std::move(Err), "lookupFlags failed");
					}
				)), TM(
					EngineBuilder().selectTarget()
				), DL(
					TM->createDataLayout()
				), ObjectLayer(
					AcknowledgeORCv1Deprecation, ES,
					[this](VModuleKey) {
						return ObjLayerT::Resources{
							std::make_shared<SectionMemoryManager>(),
							Resolver
						};
					}
				), CompileLayer(
					AcknowledgeORCv1Deprecation, ObjectLayer,
					SimpleCompiler(*TM)
				)
			{
				llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
			}

			TargetMachine &getTargetMachine() { return *TM; }

			VModuleKey addModule(std::unique_ptr<Module> M) {
				auto K = ES.allocateVModule();
				cantFail(CompileLayer.addModule(K, std::move(M)));
				ModuleKeys.push_back(K);
				return K;
			}

			void removeModule(VModuleKey K) {
				ModuleKeys.erase(find(ModuleKeys, K));
				cantFail(CompileLayer.removeModule(K));
			}

			JITSymbol findSymbol(llvm::StringRef Name) {
				return findMangledSymbol(mangle(Name));
			}

		private:
			std::string mangle(llvm::StringRef Name) {
				std::string MangledName;
				{
					llvm::raw_string_ostream MangledNameStream(MangledName);
					llvm::Mangler::getNameWithPrefix(MangledNameStream, Name, DL);
				}
				return MangledName;
			}

			JITSymbol findMangledSymbol(std::string Name) {
				#ifdef _WIN32
					// The symbol lookup of ObjectLinkingLayer uses the SymbolRef::SF_Exported
					// flag to decide whether a symbol will be visible or not, when we call
					// IRCompileLayer::findSymbolIn with ExportedSymbolsOnly set to true.
					//
					// But for Windows COFF objects, this flag is currently never set.
					// For a potential solution see: https://reviews.llvm.org/rL258665
					// For now, we allow non-exported symbols on Windows as a workaround.
					const bool ExportedSymbolsOnly = false;
				#else
					const bool ExportedSymbolsOnly = true;
				#endif

				// Search modules in reverse order: from last added to first added.
				// This is the opposite of the usual search order for dlsym, but makes more
				// sense in a REPL where we want to bind to the newest available definition.
				for (auto H : make_range(ModuleKeys.rbegin(), ModuleKeys.rend())) {
					if (auto Sym = CompileLayer.findSymbolIn(H, std::string(Name), ExportedSymbolsOnly)) {
						return Sym;
					}
				}

				// If we can't find the symbol in the JIT, try looking in the host process.
				if (auto SymAddr = RTDyldMemoryManager::getSymbolAddressInProcess(Name)) {
					return JITSymbol(SymAddr, JITSymbolFlags::Exported);
				}

				#ifdef _WIN32
					// For Windows retry without "_" at beginning, as RTDyldMemoryManager uses
					// GetProcAddress and standard libraries like msvcrt.dll use names
					// with and without "_" (for example "_itoa" but "sin").
					if (Name.length() > 2 && Name[0] == '_') {
						if (auto SymAddr = RTDyldMemoryManager::getSymbolAddressInProcess(Name.substr(1))) {
							return JITSymbol(SymAddr, JITSymbolFlags::Exported);
						}
					}
				#endif

				return nullptr;
			}

			ExecutionSession ES;
			std::shared_ptr<SymbolResolver> Resolver;
			std::unique_ptr<TargetMachine> TM;
			const DataLayout DL;
			ObjLayerT ObjectLayer;
			CompileLayerT CompileLayer;
			std::vector<VModuleKey> ModuleKeys;
	};

} }
