def include_env v
  ENV[v].to_s
end
$cc = ENV['CC'].nil? ? 'clang-3.4' : ENV['CC']
$ar = 'ar'
$ld = 'ld'
# $ldflags = "-lpthread -L. #{include_env 'LDFLAGS'}".strip
$cflags  = "-g -Wall -Wextra -Wconversion -std=c99 -I. #{include_env 'CFLAGS'}".strip

def cflags_for file
  basename = File.basename file
  cflags = $cflags
  if basename == "object.o" || basename == "generator.o"
    cflags += " #{`pkg-config --cflags glib-2.0`.strip}"
  end
  if basename == "generator.o"
    cflags += ' -Wno-unused-parameter'
  end
  return cflags
end

desc "Build"
task "build" => ["libhivem.a"]
task "default" => ["build"]

namespace "build" do
  desc "Build include directory (header files)"
  task "include" do
    headers = {
      "vm" => "hvm",
      "object" => "hvm_object",
      "symbol" => "hvm_symbol",
      "chunk"  => "hvm_chunk",
      "generator" => "hvm_generator"
    }
    headers.each do |src, dst|
      sh "cp src/#{src}.h include/#{dst}.h"
    end
  end
end

# desc "Compile"
file 'libhivem.a' => [
  # Source
  'src/vm.o', 'src/object.o', 'src/symbol.o', 'src/frame.o', 'src/chunk.o',
  'src/generator.o',
] do |t|
  # sh "cc -o #{t.name} #{t.prerequisites.join ' '} #{LDFLAGS} #{CFLAGS}"
  sh "#{$ar} rcs #{t.name} #{t.prerequisites.join ' '}"
end


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

desc "Clean up objects"
task "clean" do
  sh "rm src/*.o"
  # sh "rm test/*.o"
  sh "rm libhivem.a"
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
