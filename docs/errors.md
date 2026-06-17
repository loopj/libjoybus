# Error Handling

In libjoybus errors are negative numbered constants. As a rule of thumb, whenever there is a status parameter, or an API functions returns an integer, a negative number will imply an error.

When a function which takes a callback returns an error, the callback will never be called.

```{eval-rst}
.. c:autoenum:: joybus_error
   :file: include/joybus/errors.h
   :members:
```
