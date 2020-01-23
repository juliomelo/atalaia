using System;
using System.Collections.Generic;
using System.IO;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Threading.Tasks;

namespace atalaia.streaming.notification
{
    public class Telegram
    {
        public async Task<bool> SendVideo(string filePath, string thumbnailFilePath = null)
        {
            string token = Environment.GetEnvironmentVariable("TELEGRAM_TOKEN");
            string chatId = Environment.GetEnvironmentVariable("TELEGRAM_CHAT_ID");

            var url = $"https://api.telegram.org/bot{token}/sendVideo?chat_id={chatId}";

            Console.WriteLine(url);
            using (var http = new HttpClient())
            {
                HttpResponseMessage resp;

                using (var form = new MultipartFormDataContent())
                {
                    using (var fs = File.OpenRead(filePath))
                    {
                        using (var streamContent = new StreamContent(fs))
                        {
                            using (var fileContent = new ByteArrayContent(await streamContent.ReadAsByteArrayAsync()))
                            {
                                fileContent.Headers.ContentType = MediaTypeHeaderValue.Parse("multipart/form-data");

                                form.Add(fileContent, "video", Path.GetFileName(filePath));

                                if (thumbnailFilePath != null)
                                {
                                    using (var fsThumbNail = File.OpenRead(thumbnailFilePath))
                                    {
                                        using (var thumbnailStreamContent = new StreamContent(fsThumbNail))
                                        {
                                            using (var thumbnailContent = new ByteArrayContent(await thumbnailStreamContent.ReadAsByteArrayAsync()))
                                            {
                                                form.Add(thumbnailContent, "thumbnail", Path.GetFileName(thumbnailFilePath));
                                                resp = await http.PostAsync(url, form);
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    resp = await http.PostAsync(url, form);
                                }
                            }
                        }
                    }
                }

                Console.WriteLine(resp.StatusCode);
                Console.WriteLine(resp.Content);

                return resp.IsSuccessStatusCode;
            }
        }
    }
}
