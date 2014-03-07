LDFLAGS = "-lpthread -L."
CFLAGS  = "-g -Wall -c -std=c99 -I."

desc "Build"
task "build" => ["libhivem.a"]
task "default" => ["build"]

# desc "Compile"
file 'libhivem.a' => [
  # Source
  'src/vm.o'
] do |t|
  # sh "cc -o #{t.name} #{t.prerequisites.join ' '} #{LDFLAGS} #{CFLAGS}"
  sh "ar rcs #{t.name} #{t.prerequisites.join ' '}"
end

rule '.o' => ['.c'] do |t|
  sh "cc #{t.source} -c #{CFLAGS} -o #{t.name}"
end

desc "Clean up objects"
task "clean" do
  sh "rm -rf src/*.o"
  # sh "rm -rf test/*.o"
  sh "rm -f libhivem.a"
end

namespace "clean" do
  desc "Clean up everything (objects, docs)"
  task "all" => ["clean", "doc:clean"] do
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
