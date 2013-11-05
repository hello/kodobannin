Pod::Spec.new do |s|
  s.name         = 'kodobannin'
  s.version      = '0.0.1'
  s.summary      = 'Stuff from the guts of the band'
  s.author       = { 'André Pang' => 'andre@sayhello.com', 'John Kelley' => 'john@sayhello.com', 'Christopher Bowns' => 'cbowns@sayhello.com' }
  s.source       = { :git => 'https://github.com/hello/kodobannin.git', :tag => :head }
  s.source_files = 'peak_detect.*'
end
