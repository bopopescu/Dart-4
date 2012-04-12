/*
 * Copyright (c) 2012, the Dart project authors.
 * 
 * Licensed under the Eclipse Public License v1.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 * 
 * http://www.eclipse.org/legal/epl-v10.html
 * 
 * Unless required by applicable law or agreed to in writing, software distributed under the License
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing permissions and limitations under
 * the License.
 */
package com.google.dart.tools.core;

import org.eclipse.core.runtime.Platform;

import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.util.Properties;

/**
 * Debug/Tracing options for the {@link DartCore} plugin.
 */
public class DartCoreDebug {
  private static final String ANALYSIS_SERVER_KEY = "experimental/analysis/server";

  // Debugging / Tracing options

  public static final boolean BUILD = isOptionTrue("debug/build");
  public static final boolean DARTLIB = isOptionTrue("debug/dartlib");
  public static final boolean DEBUG_INDEX_CONTRIBUTOR = isOptionTrue("debug/index/contributor");
  public static final boolean METRICS = isOptionTrue("debug/metrics");
  public static final boolean WARMUP = isOptionTrue("debug/warmup");
  public static final boolean VERBOSE = isOptionTrue("debug/verbose");
  public static final boolean LOGGING_FROG = isOptionTrue("logging/frog");
  public static final boolean LOGGING_DEBUGGER = isOptionTrue("logging/debugger");

  public static final boolean TRACE_ARTIFACT_PROVIDER = isOptionTrue("trace/artifactProvider");
  public static final boolean TRACE_INDEX_CONTRIBUTOR = isOptionTrue("trace/index/contributor");
  public static final boolean TRACE_INDEX_PROCESSOR = isOptionTrue("trace/index/processor");

  public static final boolean ENABLE_CONTENT_ASSIST_TIMING = isOptionTrue("debug/ResultCollector");
  public static final boolean ENABLE_TYPE_REFINEMENT = isOptionTrue("debug/RefineTypes");
  public static final boolean ENABLE_MARK_OCCURRENCES = isOptionTrue("debug/markOccurrences");

  // Performance measurement and reporting options.

  public static final boolean PERF_ANALYSIS_FILESCAN = isOptionTrue("perf/analysis/filescan");
  public static final boolean PERF_INDEX = isOptionTrue("perf/index");

  // Experimental functionality options.

  public static final boolean SERVER_DEBUGGING = isOptionTrue("experimental/serverDebugging");
  public static final boolean ANALYSIS_SERVER = isRawOptionTrue(ANALYSIS_SERVER_KEY);
  public static final boolean NEW_INDEXER = true; //isOptionTrue("experimental/newIndexer");
  public static final boolean ENABLE_CALL_GRAPH = true; //NEW_INDEXER && isOptionTrue("experimental/callHierarchy");
  public static final boolean ENABLE_UPDATE = isOptionTrue("experimental/update");

  private static Properties rawOptions;

  public static void setAnalysisServerEnabled(boolean enabled) {
    readRawOptions();
    rawOptions.put(DartCore.PLUGIN_ID + "/" + ANALYSIS_SERVER_KEY, enabled ? "true" : "false");
    writeRawOptions();
  }

  private static File getRawOptionsFile() {
    return new File(Platform.getInstallLocation().getURL().getFile(), ".options");
  }

  private static boolean isOptionTrue(String optionSuffix) {
    return "true".equalsIgnoreCase(Platform.getDebugOption(DartCore.PLUGIN_ID + "/" + optionSuffix));
  }

  private static boolean isRawOptionTrue(String optionSuffix) {
    readRawOptions();
    return "true".equalsIgnoreCase(rawOptions.getProperty(DartCore.PLUGIN_ID + "/" + optionSuffix));
  }

  private static void readRawOptions() {
    if (rawOptions != null) {
      return;
    }
    rawOptions = new Properties();
    if (!getRawOptionsFile().exists()) {
      return;
    }
    try {
      FileReader reader = new FileReader(getRawOptionsFile());
      try {
        rawOptions.load(reader);
      } finally {
        reader.close();
      }
    } catch (Exception e) {
      DartCore.logError("Failed to read " + getRawOptionsFile(), e);
    }
  }

  private static void writeRawOptions() {
    try {
      FileWriter writer = new FileWriter(getRawOptionsFile());
      try {
        rawOptions.store(writer, null);
      } finally {
        writer.close();
      }
    } catch (Exception e) {
      DartCore.logError("Failed to write " + getRawOptionsFile(), e);
    }
  }
}
