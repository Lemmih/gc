{-# LANGUAGE ForeignFunctionInterface #-}

module Main (main) where

import Control.Concurrent
  ( forkOn,
    getNumCapabilities,
    myThreadId,
    threadDelay,
  )
import Control.Monad (forM_, when)
import Foreign.C (CInt (..))
import System.Console.ANSI
  ( clearFromCursorToLineEnd,
    cursorDownLine,
    cursorUpLine,
  )
import System.Environment (getArgs)
import System.IO (hFlush, stdout, hClose)
import System.IO.Unsafe (unsafePerformIO)
import System.Mem (performMajorGC)

foreign import ccall safe "sleep" safe_sleep :: CInt -> IO CInt

foreign import ccall unsafe "sleep" unsafe_sleep :: CInt -> IO CInt

main :: IO ()
main = do
  n <- getNumCapabilities
  forM_ (reverse [0 .. n -1]) $ \hec ->
    putStrLn $ "Cap " ++ show hec ++ " is blocked."
  forM_ [0 .. n -1] $ \hec -> do
    forkOn hec (counter hec)
  if hasArg "unsafe"
    then unsafe_sleep 2
    else safe_sleep 2
  hClose stdout
  return ()

counter :: Int -> IO ()
counter n = do
  threadDelay (10 ^ 5)
  when (hasArg "gc") performMajorGC
  tid <- myThreadId
  threadDelay (10 ^ 5 * n)
  cursorUpLine (n + 1)
  clearFromCursorToLineEnd
  putStr $ "Cap " ++ show n ++ " is NOT blocked."
  cursorDownLine (n + 1)
  hFlush stdout

hasArg :: String -> Bool
hasArg key = unsafePerformIO $ elem key <$> getArgs
