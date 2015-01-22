def include_env v
  ENV[v].to_s
end

# Setting up global constants
CC    = ENV['CC'].nil?  ? 'clang'   : ENV['CC']
# cpp = ENV['CPP'].nil? ? 'clang++' : ENV['CPP']
AR = 'ar'
LD = 'ld'
IS_LINUX = `uname -s`.strip == 'Linux'

# $ldflags = "-lpthread -L. #{include_env 'LDFLAGS'}".strip
warnings = '-Wall -Wextra -Wconversion'
cflags   = "-g -fPIC #{warnings} -std=c99 -I. #{include_env 'CFLAGS'}".strip

# Expose cflags and Lua globally for `cflags_for`
CFLAGS = cflags
LUA    = 'lua5.1'

llvm_config = 'llvm-config'
if `uname -s`.strip == 'Darwin'
  llvm_config = "#{`brew --prefix llvm`.strip}/bin/llvm-config"
end
LLVM_CONFIG  = llvm_config
LLVM_MODULES = 'core analysis mcjit native'
LLVM_LIBDIR  = `#{llvm_config} --libdir`.strip

version = `cat ./VERSION`.strip
shared_library = "libhivm-#{version}.so"

def cflags_for file
  basename = File.basename file
  flags = CFLAGS
  if %w{object.o generator.o exception.o debug.o bootstrap.o}.include? basename
    flags += " #{`pkg-config --cflags glib-2.0`.strip}"
  end
  if basename == 'object.o' && IS_LINUX
    flags += ' -I/usr/local/include'
  end
  # TODO: Refactor to make this unnecessary
  if basename == "generator.o" || basename == "bootstrap.o" || basename == "debug.o"
    flags += ' -Wno-unused-parameter'
  end
  if basename == "vm-db.o" || basename == "debug.o"
    flags += " -DHVM_VM_DEBUG #{`pkg-config --cflags #{LUA}`.strip}"
  end
  if basename == "jit-compiler.o"
    flags += " "+`#{LLVM_CONFIG} --cflags`.strip
  end
  # Optimization passes on some objects
  if basename =~ /^vm/ || basename == "gc1.o"
    flags += ' -O2'
  end
  return flags
end

desc "Build"
task "build" => ['libhivm.a', 'libhivm-db.a']#, "libhivm.so"]
task "default" => ["build", "build:include"]

headers = {
  "vm" => "hvm",
  "object" => "hvm_object",
  "symbol" => "hvm_symbol",
  "chunk"  => "hvm_chunk",
  "generator" => "hvm_generator",
  "bootstrap" => "hvm_bootstrap",
  "exception" => "hvm_exception",
  "debug" => "hvm_debug"
}
headers.each do |src, dst|
  file "include/#{dst}.h" => "src/#{src}.h" do |t|
    sh "cp -p #{t.prerequisites[0]} #{t.name}"
  end
end

namespace "build" do
  desc "Build include directory (header files)"
  task "include" => headers.values.map {|dst| "include/#{dst}.h" }

  desc "Build the shared library"
  task "shared" => shared_library
end

objects = [
  # Source
  'src/vm.o', 'src/object.o', 'src/symbol.o', 'src/frame.o', 'src/chunk.o',
  'src/generator.o', 'src/bootstrap.o', 'src/exception.o', 'src/gc1.o',
  'src/jit-tracer.o', 'src/jit-compiler-llvm.o',
  # Generated source
  'src/chunk.pb-c.o'
]
debug_objects = objects.map do |file|
  (file == 'src/vm.o') ? 'src/vm-db.o' : file
end
debug_objects << 'src/debug.o'

static_archiver = lambda do |t|
  # sh "cc -o #{t.name} #{t.prerequisites.join ' '} #{LDFLAGS} #{CFLAGS}"
  sh "#{AR} rcs #{t.name} #{t.prerequisites.join ' '}"
end

# Generating the static libraries
file 'libhivm.a' => objects, &static_archiver
file 'libhivm-db.a' => debug_objects, &static_archiver


# Don't use the giant LLVM-bundled JIT compiler object in the dynamic library
shared_objects = objects.map do |file|
  (file == 'src/jit-compiler-llvm.o') ? 'src/jit-compiler.o' : file
end

file shared_library => shared_objects do |t|
  objects = t.prerequisites.join ' '
  # Static libraries
  ldflags = '-liconv -lz -lpthread -ledit -lcurses -lm -lc++ '
  ldflags << `pkg-config --libs glib-2.0 lua5.1 libprotobuf-c`.strip+' '
  # Add the LLVM dynamic library
  llvm_version = `#{LLVM_CONFIG} --version`.strip
  ldflags << "-L#{LLVM_LIBDIR} -lLLVM-#{llvm_version} "
  if `uname -s`.strip == 'Darwin'
    ldflags += " -macosx_version_min 10.10"
  end
  sh "#{LD} #{objects} #{ldflags} -dylib -o #{t.name}"
end


file 'src/jit-compiler-llvm.o' => 'src/jit-compiler.o' do |t|
  llvm_libs    = `#{LLVM_CONFIG} --libs #{LLVM_MODULES}`.gsub("\n", '').strip
  llvm_ldflags = "-L#{LLVM_LIBDIR} #{llvm_libs}"
  # -r remerges into new file
  sh "#{LD} #{t.prerequisites.first} #{llvm_ldflags} -r -o #{t.name}"
end

# file 'src/object-jemalloc.o' => 'src/object.o' do |t|
#   # Remerge to pull in jemalloc
#   jemalloc = find_jemalloc
#   sh "#{LD} #{t.prerequisites.first} #{jemalloc} -r -o #{t.name}"
# end

file "src/chunk.pb-c.c" => ["src/chunk.proto"] do |t|
  sh "protoc-c --c_out=. #{t.prerequisites.first}"
end

# Let Rake know that debug.o depends on debug-lua.include.c
file 'src/debug.o' => 'src/debug-lua.include.c'
# Ditto for vm-dispatch
file 'src/vm.o' => ['src/vm.c', 'src/vm-dispatch.include.c']

# Generic compilation of object files
rule '.o' => ['.c'] do |t|
  sh "#{CC} #{t.source} -c #{cflags_for t.name} -o #{t.name}"
  # if File.basename(t.name) == "object.o"
  #   # -r merges object files into a new object file.
  #   # TODO: Make this platform-independent (fiddle with pkg-config?)
  #   libs = [
  #     "/usr/local/Cellar/glib/2.38.2/lib/libglib-2.0.a",
  #     "/usr/local/Cellar/gettext/0.18.3.2/lib/libintl.a"
  #   ]
  #   sh "#{ld} -r #{t.name} #{libs.join ' '} -o #{t.name}"
  # end
  jemalloc = find_jemalloc
  if File.basename(t.name) == 'object.o'
    # a.o -> a.tmp.o
    tmp = t.name.sub /\.o$/, '.tmp.o'
    sh "mv #{t.name} #{tmp}"
    sh "#{LD} #{tmp} #{jemalloc} -r -o #{t.name}"
  end
end

# Compiling the debug version of vm.c (include the dispatcher as a dependency)
file 'src/vm-db.o' => ['src/vm.c', 'src/vm-dispatch.include.c'] do |t|
  sh "#{CC} #{t.prerequisites.first} -c #{cflags_for t.name} -o #{t.name}"
end

desc "Clean up objects"
task "clean" do
  sh "rm -f src/*.o"
  sh "rm -f src/chunk.pb-c.*"
  sh "rm -f include/*.h"
  # sh "rm test/*.o"
  sh "rm -f lib*.*"
end

namespace "clean" do
  desc "Clean up everything (objects, docs)"
  task "all" => ["clean", "doc:clean"] do
    sh "rm include/*.h"
    puts 'Done!'
  end
end

desc "Build documentation"
task "doc" do
  sh "doxygen Doxyfile"
end

namespace "doc" do
  desc "Clean up documentation"
  task "clean" do
    sh "rm -rf doc/*"
  end
end


# Utility functions:

def find_jemalloc
  paths = [
    '/usr/local/lib/libjemalloc.a',
    '/usr/local/libjemalloc.a'
  ]
  paths.each do |p|
    return p if File.exists? p
  end
  raise 'libjemalloc.a not found'
end
