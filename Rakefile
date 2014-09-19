def include_env v
  ENV[v].to_s
end
$cc = ENV['CC'].nil? ? 'clang' : ENV['CC']
$ar = 'ar'
$ld = 'ld'
# $ldflags = "-lpthread -L. #{include_env 'LDFLAGS'}".strip
$cflags  = "-g -fPIC -Wall -Wextra -Wconversion -std=c99 -I. #{include_env 'CFLAGS'}".strip

$lua = 'lua5.1'
# if `uname -s`.strip == 'Darwin'
#   $lua = 'lua'
# end

def cflags_for file
  basename = File.basename file
  cflags = $cflags
  if basename == "object.o" || basename == "generator.o" || basename == "exception.o" || basename == "debug.o"
    cflags += " #{`pkg-config --cflags glib-2.0`.strip}"
  end
  if basename == "libhivm.so"
    cflags += " #{`pkg-config --libs libprotobuf-c`.strip} #{`pkg-config --libs glib-2.0`.strip}"
  end
  # TODO: Refactor to make this unnecessary
  if basename == "generator.o" || basename == "bootstrap.o" || basename == "debug.o"
    cflags += ' -Wno-unused-parameter'
  end
  if basename == "vm-db.o" || basename == "debug.o"
    cflags += " -DHVM_VM_DEBUG #{`pkg-config --cflags #{$lua}`.strip}"
  end
  return cflags
end

desc "Build"
task "build" => ["libhivm.a", "libhivm-db.a"]#, "libhivm.so"]
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
end

objects = [
  # Source
  'src/vm.o', 'src/object.o', 'src/symbol.o', 'src/frame.o', 'src/chunk.o',
  'src/generator.o', 'src/bootstrap.o', 'src/exception.o', 'src/gc1.o',
  'src/jit-tracer.o',
  # Generated source
  'src/chunk.pb-c.o'
]
debug_objects = objects.map do |file|
  (file == 'src/vm.o') ? 'src/vm-db.o' : file
end
debug_objects << 'src/debug.o'

static_archiver = lambda do |t|
  # sh "cc -o #{t.name} #{t.prerequisites.join ' '} #{LDFLAGS} #{CFLAGS}"
  sh "#{$ar} rcs #{t.name} #{t.prerequisites.join ' '}"
end

# Generating the static libraries
file 'libhivm.a' => objects, &static_archiver
file 'libhivm-db.a' => debug_objects, &static_archiver


file 'libhivm.so' => objects do |t|
  sh "#{$cc} #{t.prerequisites.join ' '} #{cflags_for t.name} -shared -o #{t.name}"
end


file "src/chunk.pb-c.c" => ["src/chunk.proto"] do |t|
  sh "protoc-c --c_out=. #{t.prerequisites.first}"
end

# Let Rake know that debug.o depends on debug-lua.include.c
file 'src/debug.o' => 'src/debug-lua.include.c'
# Ditto for vm-dispatch
file 'src/vm.o' => 'src/vm-dispatch.include.c'

# Generic compilation of object files
rule '.o' => ['.c'] do |t|
  sh "#{$cc} #{t.source} -c #{cflags_for t.name} -o #{t.name}"
  # if File.basename(t.name) == "object.o"
  #   # -r merges object files into a new object file.
  #   # TODO: Make this platform-independent (fiddle with pkg-config?)
  #   libs = [
  #     "/usr/local/Cellar/glib/2.38.2/lib/libglib-2.0.a",
  #     "/usr/local/Cellar/gettext/0.18.3.2/lib/libintl.a"
  #   ]
  #   sh "#{$ld} -r #{t.name} #{libs.join ' '} -o #{t.name}"
  # end
end

# Compiling the debug version of vm.c (include the dispatcher as a dependency)
file 'src/vm-db.o' => ['src/vm.c', 'src/vm-dispatch.include.c'] do |t|
  sh "#{$cc} #{t.prerequisites.first} -c #{cflags_for t.name} -o #{t.name}"
end

desc "Clean up objects"
task "clean" do
  sh "rm -f src/*.o"
  sh "rm -f src/chunk.pb-c.*"
  sh "rm -f include/*.h"
  # sh "rm test/*.o"
  sh "rm -f libhivm.*"
  sh "rm -f libhivm-db.a"
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
