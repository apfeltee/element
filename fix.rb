
def fix(file)
  mustwrite = 0
  data = File.read(file)
  newdata = []
  data.each_line do |ln|
    m = ln.match(/^\s*#\s*include\s*"(?<filename>.*?)"/)
    if m then
      incf = m["filename"]
      newf = "element.h"
      if ((not File.file?(incf)) && File.file?(newf)) then
        newdata.push(sprintf("#include %p\n", newf))
        mustwrite += 1
      else
        newdata.push(ln)
      end
    else
      newdata.push(ln)
    end
  end
  if mustwrite > 0 then
    File.open(file, "wb") do |ofh|
      newdata.each do |ln|
        ofh.write(ln)
      end
    end
    $stderr.printf("fixed %p (%d changes)\n", file, mustwrite)
  end
end

begin
  (Dir.glob("*.cpp") + Dir.glob("*.h")).each do |file|
    fix(file)
  end
end

