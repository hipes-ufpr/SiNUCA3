SINUCA_TRACER_DIR = ../src/tracer/sinuca/
PINTOOL_UTILS_DIR = ./utils/
TOOL_ROOTS = sinuca3_pintool
FILE_HANDLER = file_handler
PINTOOL_UTILS = dynamic_trace_writer \
				memory_trace_writer \
				static_trace_writer
OBJ_DEPS = $(OBJDIR)$(TOOL_ROOTS)$(OBJ_SUFFIX) \
		$(OBJDIR)$(FILE_HANDLER)$(OBJ_SUFFIX) \
		$(addprefix $(OBJDIR),$(addsuffix $(OBJ_SUFFIX),$(PINTOOL_UTILS))) \

# This section contains the build rules for all binaries that have special build rules.
# See makefile.default.rules for the default build rules.

$(OBJDIR)$(TOOL_ROOTS)$(OBJ_SUFFIX): $(TOOL_ROOTS).cpp
	$(CXX) $(TOOL_CXXFLAGS) -I../src -I. $(COMP_OBJ)$@ $<

$(OBJDIR)$(FILE_HANDLER)$(OBJ_SUFFIX): $(SINUCA_TRACER_DIR)$(FILE_HANDLER).cpp
	$(CXX) $(TOOL_CXXFLAGS) -I../src -I. $(COMP_OBJ)$@ $<

$(OBJDIR)%_writer$(OBJ_SUFFIX): $(PINTOOL_UTILS_DIR)%_writer.cpp
	$(CXX) $(TOOL_CXXFLAGS) -I../src -I. $(COMP_OBJ)$@ $<

$(OBJDIR)$(TOOL_ROOTS)$(PINTOOL_SUFFIX): $(OBJ_DEPS)
	$(LINKER) $(TOOL_LDFLAGS) $(LINK_EXE)$@ $^ $(TOOL_LPATHS) $(TOOL_LIBS)
